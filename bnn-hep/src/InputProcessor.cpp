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
#include <map>
#include <sstream>
#include <cstdio>


using namespace std;


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
    
    
    // Prepare a map to store indices of events tried for training. It binds a vector with
    //indices to source file's name. Note that the code can handle cases when the same source file
    //appears several times
    map<string, vector<unsigned long>> trainEventsIndices;
    
    
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
            
            
            // Memorize the list of events tried for training to write it down later. There is no
            //guarantee that this operation does not insert duplicates into the vector
            auto &trainListCurFile = trainEventsIndices[sample.fileName];
            trainListCurFile.reserve(trainListCurFile.size() + nEventsTriedForTraining);
            trainListCurFile.insert(trainListCurFile.end(), eventsForTraining.begin(),
             eventsForTraining.end());
        }
        else
        // The user specified the desired number of events in the training set only
        {
            // A vector to define the order according to which the tree will be read
            vector<unsigned long> eventsToRead;
            eventsToRead.reserve(nEntries);
            
            
            // The current source file might have already been read while processing one of the
            //previous samples. It makes sense if only both signal and background events are
            //taken from the same file with orthogonal selections. Since an event is marked to
            //have been tried for training before the selection is evaluated, the same events
            //should be tried again with the current event selection. Otherwise we waste events by
            //marking them as tried for training and then ignoring because they fail signal or
            //background selection
            auto &trainListCurFile = trainEventsIndices[sample.fileName];
            eventsToRead.insert(eventsToRead.end(), trainListCurFile.begin(),
             trainListCurFile.end());
            
            
            // But there is no guarantee that the desired number of events will be found while
            //looping over trainListCurFile. To deal with it, extend vector eventsToRead with a
            //randomly shuffled vector off all indices that are not yet included in trainListCurFile
            // First, find this complementary set of indices (note that trainListCurFile is ordered)
            vector<unsigned long> untestedEvents;
            untestedEvents.reserve(nEntries - trainListCurFile.size());
            
            
            if (trainListCurFile.size() > 0)
            {
                for (unsigned index = 0; index < trainListCurFile.front(); ++index)
                    untestedEvents.push_back(index);
                
                for (unsigned k = 0; k < trainListCurFile.size() - 1; ++k)
                    for (unsigned index = trainListCurFile.at(k) + 1;
                     index < trainListCurFile.at(k + 1); ++index)
                        untestedEvents.push_back(index);
                //^ The algorithm is tolerant to duplicates in trainListCurFile
                
                for (unsigned index = trainListCurFile.back() + 1; index < nEntries; ++index)
                    untestedEvents.push_back(index);
            }
            else
                for (unsigned index = 0; index < nEntries; ++index)
                    untestedEvents.push_back(index);
            
            
            // Shuffle the vector of complementary events
            std::random_shuffle(untestedEvents.begin(), untestedEvents.end(), RandomInt);
            
            
            // Extend the vector of reading order. Now it contains exactly nEntries events
            eventsToRead.insert(eventsToRead.end(), untestedEvents.begin(), untestedEvents.end());
            
            
            // Read the tree in the specified order and fill the local training set
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
            
            
            // If needed, extend the list of events tried for training for the current source file
            if (nEntriesRead > trainListCurFile.size())  // some of untestedEvents were read
            {
                trainListCurFile.reserve(nEntriesRead);
                trainListCurFile.insert(trainListCurFile.end(), untestedEvents.begin(),
                 untestedEvents.begin() + (nEntriesRead - trainListCurFile.size()));
            }
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
        
        
        // Sort the vector of indices of events tried for training
        auto &trainListCurFile = trainEventsIndices[sample.fileName];
        sort(trainListCurFile.begin(), trainListCurFile.end());
        
        
        // Now append the local training set to the global list
        trainingSet.splice(trainingSet.end(), localTrainingSet);
    }
    
    
    // Now write indices of events tried for training to a text file. First, create an object to
    //manage the writing
    TrainEventList writeTrainEvents(config.GetTaskName() + "_trainEvents.txt",
     TrainEventList::Mode::Write);
    
    // Loop over the map with vectors of indices of events tried for training and write them to the
    //file
    for (auto const &val : trainEventsIndices)
        writeTrainEvents.WriteList(val.first, val.second.begin(), val.second.end());
    
    
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
