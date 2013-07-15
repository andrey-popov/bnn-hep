#include "InputProcessor.hpp"
#include "TrainEventList.hpp"
#include "TransformStandard.hpp"
#include "TransformGauss.hpp"
#include "TransformPCA.hpp"
#include "utility.hpp"

#include <TFile.h>
#include <TTree.h>
#include <TFriendElement.h>

#include <algorithm>
#include <sstream>
#include <cstdio>


unsigned InputProcessor::Event::nVars = 0;


InputProcessor::InputProcessor(Logger &log_, Config const &config_):
    log(log_), config(config_),
    trainingFileName(config.GetTaskName() + "_trainFile_" + GetRandomName() + ".root")
{
    // All the processing is actually done here
    BuildTrainingSet();
    TransformInputs();
    WriteTrainFile();
}


InputProcessor::~InputProcessor()
{
    for (auto const &t: transforms)
        delete t;
    
    if (not config.GetKeepTempFiles())
    {
        std::remove(trainingFileName.c_str());
        log << info(2) << "Temporary file \"" << trainingFileName << "\" removed." << eom;
    }
}


void InputProcessor::BuildTrainingSet()
{
    vector<string> const &varNames = config.GetVariables();
    Event::nVars = varNames.size();
    
    // Object to write the list of the events tried for the training set to a file
    TrainEventList writeTrainEvents(config.GetTaskName() + "_trainEvents.txt",
     TrainEventList::Mode::Write);
    
    // Loop over all the input samples
    for (Config::Sample const &sample : config.GetSamples())
    {
        // Open the file and construct the source tree
        TFile srcFile(sample.fileName.c_str());
        
        if (srcFile.IsZombie())
        {
            log << critical << "Input file \"" << sample.fileName << "\" is not found or is not " <<
             "a valid ROOT file." << eom;
            exit(1);
        }
        
        auto treeIt = sample.trees.cbegin();
        TTree *srcTree = dynamic_cast<TTree *>(srcFile.Get(treeIt->c_str()));
        
        if (srcTree == nullptr)
        {
            log << critical << "Tree \"" << *treeIt << "\" is not found in file \"" <<
             sample.fileName << "\"." << eom;
            exit(1);
        }
        
        for (++treeIt; treeIt != sample.trees.cend(); ++treeIt)
        {
            TFriendElement * const fe = srcTree->AddFriend(treeIt->c_str());
            
            if (fe->GetTree() == nullptr)
            {
                log << critical << "Tree \"" << *treeIt << "\" is not found in file \"" <<
                 sample.fileName << "\"." << eom;
                exit(1);
            }
        }
        
        unsigned long const nEntries = srcTree->GetEntries();
        
        
        // The formulas to be evaluated when reading the tree
        TTreeFormula *weight =
         new TTreeFormula(sample.trainWeight.c_str(), sample.trainWeight.c_str(), srcTree);
        
        if (weight->GetNdim() == 0)  // TTreeFormula sets it to zero in case of error
        {
            log << critical << "Input variable \"" << sample.trainWeight << "\" cannot be " <<
             "evaluated (wrong branch name or syntax)." << eom;
            exit(1);
        }
        
        vector<TTreeFormula *> vars(Event::nVars);
        
        for (unsigned i = 0; i < Event::nVars; ++i)
        {
            vars.at(i) = new TTreeFormula(varNames.at(i).c_str(), varNames.at(i).c_str(), srcTree);
            
            if (vars.at(i)->GetNdim() == 0)  // TTreeFormula sets it to zero in case of error
            {
                log << critical << "Input variable \"" << varNames.at(i) << "\" cannot be " <<
                 "evaluated (wrong branch name or syntax)." << eom;
                exit(1);
            }
        }
        
        
        // The training set made from the current file only
        list<Event> localTrainingSet;
        unsigned long nEventsTriedForTraining = 0;
        
        if (sample.trainEventsFileName.length() > 0)
        // The training set is specified with a file
        {
            TrainEventList readTrainEvents(sample.trainEventsFileName, TrainEventList::Mode::Read);
            readTrainEvents.ReadList(sample.fileName);
            auto const &eventsForTraining = readTrainEvents.GetReadEvents();
            
            // Read the requested events from the tree
            for (auto const &ev: eventsForTraining)
            {
                srcTree->LoadTree(ev);
                Double_t const weightValue = weight->EvalInstance();
                
                if (weightValue != 0.)
                    localTrainingSet.emplace_back(sample.type, weightValue, vars);
            }
            
            nEventsTriedForTraining = eventsForTraining.size();
            
            
            // Write the list of event tried for training
            writeTrainEvents.WriteList(sample.fileName, eventsForTraining.begin(),
             eventsForTraining.end());
        }
        else
        // The used specified the desired number of events in the training set only
        {
            // The tree will be read in a random way. Prepare a shuffled vector of indices to
            //perform it
            vector<unsigned long> eventsToRead(nEntries);
            
            for (unsigned long i = 0; i < nEntries; ++i)
                eventsToRead.at(i) = i;
            
            std::random_shuffle(eventsToRead.begin(), eventsToRead.end(), RandomInt);
            
            // Read the tree and fill the local training set
            unsigned long nEntriesRead = 0;
            
            while (nEntriesRead < nEntries * sample.maxFractionTrainEvents and
             nEntriesRead < nEntries)
            {
                srcTree->LoadTree(eventsToRead.at(nEntriesRead));
                Double_t const weightValue = weight->EvalInstance();
                
                if (weightValue != 0.)
                    localTrainingSet.emplace_back(sample.type, weightValue, vars);
                
                ++nEntriesRead;
                
                if (localTrainingSet.size() == sample.maxTrainEvents)
                    break;
            }
            
            nEventsTriedForTraining = nEntriesRead;
            
            
            // Write the list of event tried for training
            writeTrainEvents.WriteList(sample.fileName, eventsToRead.begin(),
             eventsToRead.begin() + nEntriesRead);
        }
        
        
        // Free memory for the formulas and the tree
        for (auto const &v : vars)
            delete v;
        
        delete weight;
        delete srcTree;
        
        
        // Not all the events in the sample will be used for training. Hence the weights should be
        //corrected
        double const weightCorrFactor = double(nEntries) / nEventsTriedForTraining;
        
        for (Event &event : localTrainingSet)
            event.weight *= weightCorrFactor;
        
        
        // Now append the local training set to the global list
        trainingSet.splice(trainingSet.end(), localTrainingSet);
    }
    
    
    // The weights should be additionally rescaled. One reason is that the combined p.d.f. in the
    //training set must be properly normalized after the data-to-simulation corrections are included
    //(it might happen that the event selection biases the average weight to be different from 1, in
    //which case the emulated data-like p.d.f. is no longer normalized to 1).
    //Another point is that one usually wants to alter the balance between the signal and the
    //background in such a way that they have equal impacts to the training.
    
    // Here I sacrifice the computational efficiency to the clearness and perform an additional loop
    unsigned long nEvents = 0;
    double sumWeights[2] = {0., 0.};
    
    for (Event const &event : trainingSet)
    {
        ++nEvents;
        sumWeights[event.type] += event.weight;  // type == 1 for signal and 0 for background
    }
    
    // Loop again and rescale the weights
    switch (config.GetReweightingType())
    {
        case Config::Reweighting::OneToOne:
        {
            double const corrFactors[2] =
             {0.5 * nEvents / sumWeights[0], 0.5 * nEvents / sumWeights[1]};
            
            for (Event &event : trainingSet)
                event.weight *= corrFactors[event.type];
        }
        break;
        
        case Config::Reweighting::Common:
        {
            double const corrFactor = nEvents / (sumWeights[0] + sumWeights[1]);
            
            for (Event &event : trainingSet)
                event.weight *= corrFactor;
        }
        break;
        
        default:
            // This should never happen
            break;
    }
            
    
    
    log << info(2) << "The events for training set (" << trainingSet.size() << " in total) are " <<
     "selected and read." << eom;
    log << info(0) << "The indices of the events tried for training are written in file \"" <<
     writeTrainEvents.GetFileName() << "\"." << eom;
}


void InputProcessor::TransformInputs()
{
    // Create the transformations
    for (auto const &code : config.GetTransformations())
        switch (code)
        {
            case Config::InputTransformation::Standard:
                transforms.push_back(new TransformStandard(log, Event::nVars));
                break;
            
            case Config::InputTransformation::Gauss:
                transforms.push_back(new TransformGauss(log, Event::nVars));
                break;
            
            case Config::InputTransformation::PCA:
                transforms.push_back(new TransformPCA(log, Event::nVars));
                break;
            
            default:
                // This should never be executed
                break;
        }
    
    
    //TODO: Check the list for pathologies, i.e. applying PCA without gaussianisation.
    
    
    // Loop over all the transformations
    for (auto &transform : transforms)
    {
        // Loop over the training set to build the transformation
        for (auto const &event : trainingSet)
            transform->AddEvent(event.weight, event.vars);
        
        transform->BuildTransformation();
        
        // Loop over the training set again and apply the transformation
        for (auto &event : trainingSet)
            transform->ApplyTransformation(event.vars);
    }
    
    log << info(1) << "The transformations of input variables are built and applied." << eom;
}


void InputProcessor::WriteTrainFile() const
{
    TFile outFile(trainingFileName.c_str(), "recreate");
    TTree *outTree = new TTree("Vars", "Tree containing the training set");
    
    Double_t target, weight;
    Double_t *vars = new Double_t[Event::nVars];
    
    outTree->Branch("target", &target);  // the first branch is the target
    outTree->Branch("weight", &weight);  // the second branch is the weight
    
    for (unsigned iVar = 0; iVar < Event::nVars; ++iVar)
    {
        std::ostringstream ost;
        ost << "var" << iVar + 1;
        
        outTree->Branch(ost.str().c_str(), vars + iVar);
    }
    
    
    // Fill the tree
    for (auto const &event : trainingSet)
    {
        target = double(event.type);  // FBM requires it to be double
        weight = event.weight;
        std::copy(event.vars, event.vars + Event::nVars, vars);
        
        outTree->Fill();
    }
    
    
    outTree->Write("", TObject::kOverwrite);
    outFile.Close();  // outTree is deleted here, too
    delete [] vars;
    
    log << info(2) << "The training set is written in file \"" << trainingFileName << "\"." << eom;
}


unsigned InputProcessor::GetDim() const
{
    return Event::nVars;
}


string const & InputProcessor::GetTrainFileName() const
{
    return trainingFileName;
}


list<TransformBase *> const & InputProcessor::GetTransformations() const
{
    return transforms;
}
