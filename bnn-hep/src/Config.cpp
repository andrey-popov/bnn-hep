#include "Config.hpp"
#include "utility.hpp"

#include <cstdlib>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>


using std::exit;


Config::Config(string const &fileName, Logger &log_):
    log(log_)
{
    // Read the configuration file
    try
    {
        cfg.readFile(fileName.c_str());
    }
    catch (libconfig::FileIOException)
    {
        log << critical << "Cannot read the configuration file." << eom;
        exit(1);
    }
    catch (libconfig::ParseException excp)
    {
        log << critical << "Syntax error in the configuration file at line " << excp.getLine() <<
         "." << eom;
        exit(1);
    }
    
    
    // Adjust the logger's verbosity level
    unsigned const verbosity = ReadParameterDef("general.verbosity", -1);
    log.SetStdVerbosity(verbosity);
    log.SetFileVerbosity(verbosity);
    
    
    
    // Read the group 'general' (though not much is left)
    size_t strIndex = fileName.find_last_of('/');
    taskName = ReadParameterDef("general.task-name", fileName.substr(strIndex + 1,
     fileName.find_last_of('.') - strIndex - 1));
    FBMPath = ReadParameterDef("general.fbm-path", string(""));
    
    if (FBMPath.length() > 0 and not boost::iends_with(FBMPath.c_str(), "/"))
        FBMPath += '/';
    
    keepTempFiles = ReadParameterDef("general.keep-temp-files", false);
    
    
    
    // Read the section with the input samples
    Setting const &stgVariables = LookupMandatorySetting("input-samples.variables");
    
    if (not stgVariables.isArray())
    {
        log << critical << "Setting \"" << stgVariables.getPath() << "\" must be an array." << eom;
        exit(1);
    }
    
    for (int i = 0; i < stgVariables.getLength(); ++i)
        variables.push_back(ReadChildParameter(stgVariables, i, string()));
    
    
    // Read the default weights and tree names
    string const defTrainWeight = ReadParameterDef("input-samples.def-train-weight", string("1"));
    string const defExamWeight = ReadParameterDef("input-samples.def-exam-weight", string("1"));
    
    vector<string> defTreeNames;
    
    if (cfg.exists("input-samples.def-trees"))
    {
        Setting const &stgTrees = LookupSetting("input-samples.def-trees");
        
        if (not stgTrees.isArray())
        {
            log << critical << "Setting \"" << stgTrees.getPath() << "\" must be an array." << eom;
            exit(1);
        }
        
        for (int i = 0; i < stgTrees.getLength(); ++i)
            defTreeNames.push_back(ReadChildParameter(stgTrees, i, string()));
    }
    
    // Parse the list of samples
    ReadSamples("input-samples.signal-samples", 1, defTrainWeight, defExamWeight, defTreeNames);
    ReadSamples("input-samples.background-samples", 0, defTrainWeight, defExamWeight, defTreeNames);
    
    /*log << info << "Samples: ";
    
    for (auto const &s: samples)
    {
        log << "(" << s.fileName << ", " << s.trees[0] << ", " << s.trainWeight << ", " <<
         s.maxTrainEvents << ", " << s.maxFractionTrainEvents << "); ";
    }
    
    log << eom;*/
    
    
    // Check for pathologies
    auto samplesBegin = samples.cbegin(), samplesEnd = samples.cend();
    
    if (std::find_if(samplesBegin, samplesEnd, [](Sample s){return (s.type == 1);}) == samplesEnd or
     std::find_if(samplesBegin, samplesEnd, [](Sample s){return (s.type == 0);}) == samplesEnd)
    {
        log << error << "At least one sample of each class must be provided." << eom;
        exit(1);
    }
    
    if (std::find_if(samplesBegin, samplesEnd,
     [](Sample s){return (s.trees.size() == 0);}) != samplesEnd)
    {
        log << error << "The source trees are not specified for one or more sample." << eom;
        exit(1);
    }
    
    
    // Read the parameters for reprocessing
    if (cfg.exists("input-samples.preprocessing"))
    {
        Setting const &stgTransforms = LookupSetting("input-samples.preprocessing");
        
        if (not stgTransforms.isArray())
        {
            log << critical << "Setting \"" << stgTransforms.getPath() << "\" must be an array." <<
             eom;
            exit(1);
        }
        
        for (int i = 0; i < stgTransforms.getLength(); ++i)
        {
            string const textTransform = ReadChildParameter(stgTransforms, i, string());
            
            if (boost::iequals(textTransform, "standard"))
                inputTransformations.push_back(InputTransformation::Standard);
            else if (boost::iequals(textTransform, "gauss"))
                inputTransformations.push_back(InputTransformation::Gauss);
            else if (boost::iequals(textTransform, "pca"))
                inputTransformations.push_back(InputTransformation::PCA);
            else
            {
                log << critical << "Preprocessing \"" << textTransform << "\" in setting \"" <<
                 stgTransforms.getPath() << "\" is not known." << eom;
                exit(1);
            }
        }
    }
    else
    {
        // Default preprocessing
        inputTransformations.push_back(InputTransformation::Gauss);
        //inputTransformations.push_back(InputTransformation::PCA);
    }
    
    
    
    // Read the section on the BNN training
    networkName = ReadParameterDef("bnn-parameters.network-name",
     taskName + "_" + GetRandomName() + ".net");
    
    // Create the directories for the network file if they do not exist
    boost::filesystem::path const networkPath(networkName);
    
    if (networkPath.has_parent_path())
        boost::filesystem::create_directories(networkPath.parent_path());
    
    // The short name which is used to construct the names of some other files
    networkShortName = networkPath.filename().stem().native();
    
    // The reweighting type
    string const reweightingTypeText = ReadParameterDef("bnn-parameters.rescale-weights",
     string("1:1"));
    
    if (reweightingTypeText.compare("1:1") == 0)
        reweightingType = Reweighting::OneToOne;
    else if (reweightingTypeText.compare("common") == 0)
        reweightingType = Reweighting::Common;
    else
    {
        log << error << "An unexpected value \"" << reweightingTypeText <<
         "\" is specified for \"bnn-parameters.rescale-weights\" parameter." << eom;
        exit(1);
    }
    
    numberNeurons = ReadParameter("bnn-parameters.number-neurons", unsigned());
    networkHyperparameters = ReadParameterDef("bnn-parameters.network-hyperparameters",
     string("- 0.05:0.5 0.05:0.5 - x0.05:0.5 - 100"));
    networkGenerationParameters = ReadParameterDef("bnn-parameters.network-generation-parameters",
     string("fix 0.5"));
    MCMCParametersFirstIt = ReadParameterDef("bnn-parameters.mcmc-parameters-first-iteration",
     string("repeat 10 sample-noise heatbath hybrid 100:10 0.2"));
    MCMCParameters = ReadParameterDef("bnn-parameters.mcmc-parameters",
     string("repeat 10 sample-sigmas heatbath 0.95 hybrid 100:10 0.3 negate"));
    burnInIterations = ReadParameterDef("bnn-parameters.burn-in", unsigned(0));
    numberIterations = ReadParameter("bnn-parameters.ensemble-size", unsigned()) + burnInIterations;
    
    
    
    // Read the section on the output C++ code for BNN
    // This part of configuration is currently hidden
    networkCPPFileName = ReadParameterDef("write-bnn.file-name", taskName + ".hpp");
    boost::filesystem::path const networkCPPFilePath(networkCPPFileName);
    
    if (networkCPPFilePath.has_parent_path())
        boost::filesystem::create_directories(networkCPPFilePath.parent_path());
    
    
    log << info(2) << "The configuration file is parsed and checked." << eom;
}


Setting const & Config::LookupSetting(string const &path) throw(SettingNotFoundException)
{
    return ExpandSetting(cfg.lookup(path));
}


Setting const & Config::LookupMandatorySetting(string const &path) throw()
{
    try
    {
        return LookupSetting(path);
    }
    catch (SettingNotFoundException excp)
    {
        log << critical << "Mandatory setting \"" << excp.getPath() << "\" is not found " <<
         "in the configuration." << eom;
        exit(1);
    }
}


Setting const & Config::ExpandSetting(Setting const &setting) throw(SettingNotFoundException)
{
    if (setting.getType() == Setting::Type::TypeString)
    {
        string const value = static_cast<char const *>(setting);
        
        if (value.length() > 0 and value[0] == '@')
            return ExpandSetting(cfg.lookup(value.substr(1)));  // recursion is allowed
    }
    
    return setting;
}


void Config::ReadSamples(string const &path, unsigned type, string const &defTrainWeight,
 string const &defExamWeight, vector<string> const &defTreeNames)
{
    Setting const &list = LookupMandatorySetting(path);
    
    if (not list.isList())
    {
        log << critical << "Setting \"" << list.getPath() << "\" must be a list." << eom;
        exit(1);
    }
    
    
    // Loop over all the samples in the list
    for (int i = 0; i < list.getLength(); ++i)
    {
        Setting const &item = list[i];
        
        if (not item.isGroup())
        {
            log << critical << "Setting \"" << list.getPath() << "\" must be a group." << eom;
            exit(1);
        }
        
        Sample sample;
        sample.type = type;
        sample.fileName = ReadChildParameter(item, "file-name", string());
        sample.trainWeight = ReadChildParameterDef(item, "train-weight", defTrainWeight);
        sample.examWeight = ReadChildParameterDef(item, "exam-weight", defExamWeight);
        
        
        // Read the list of trees
        if (not item.exists("trees"))
            sample.trees = defTreeNames;
        else
        {
            Setting const &stgTrees = LookupSetting(item.getPath() + ".trees");
            
            if (not stgTrees.isArray())
            {
                log << critical << "Setting \"" << stgTrees.getPath() << "\" must be an array."
                 << eom;
                exit(1);
            }
            
            for (int i = 0; i < stgTrees.getLength(); ++i)
                sample.trees.push_back(ReadChildParameter(stgTrees, i, string()));
        }
        
        
        // The user can either set a limination on the desired number of events to be used for
        //training or specify a text file with the event list
        if (item.exists("event-list-file"))
        // The user uses file to specify the training set
        {
            // Make sure the user did not specify both the limitations on the number of events and
            //the event list in a text file. If (s)he did, take the event list only
            if (item.exists("number-events"))
                log << warning << "Both \"event-list-file\" and \"number-events\" options are " <<
                 "specified for \"" << item.getPath() << "\" setting. The former only will be " <<
                 "considered." << eom;
            
            sample.maxTrainEvents = -1;
            sample.maxFractionTrainEvents = 1.;
            sample.trainEventsFileName = ReadChildParameter(item, "event-list-file", string());
            
            // Check the file exists
            if (not boost::filesystem::exists(sample.trainEventsFileName))
            {
                log << error << "The file \"" << sample.trainEventsFileName << "\" specified in " <<
                 "section \"" << item.getPath() << ".event-list-file\" is not found." << eom;
                exit(1);
            }
        }
        else if (item.exists("number-events"))
        // The user requests a number of events in the training set only
        {
            Setting const &stgNumEvents = LookupSetting(item.getPath() + ".number-events");
            int const numEventsSize = stgNumEvents.getLength();
            
            if (numEventsSize == 0 /*or numEventsSize > 2*/)
            {
                log << critical << "Setting \"" << stgNumEvents.getPath() << "\" must contain " <<
                 "at least one element." << eom;
                exit(1);
            }
            else if (numEventsSize > 2)
            {
                log << warning << "Setting \"" << stgNumEvents.getPath() << "\" contains more " <<
                 "than two elements which is unexpected. The smaller one in each type will be " <<
                 "chosen" << eom;
            }
            
            
            // Parse the limitations on the size of the training set
            bool simpleNumberFound = false, fractionFound = false;
            
            for (int i = 0; i < numEventsSize; ++i)
            {
                string number = ReadChildParameter(stgNumEvents, i, string());
                //boost::trim(number);  // boost::range is not compatible with C++11
                
                if (boost::iends_with(number.c_str(), "%"))  // this is a fraction
                {
                    number = number.substr(0, number.length() - 1);
                    double const fraction = atof(number.c_str()) / 100.;
                    
                    if (fraction <= 0.)
                    {
                        log << error << "Setting \"" << stgNumEvents.getPath() << "[" << i <<
                         "]\" makes the training set empty. The number might be misformated." <<
                         eom;
                    }
                    
                    if (not fractionFound or fraction < sample.maxFractionTrainEvents)
                        sample.maxFractionTrainEvents = fraction;
                    
                    fractionFound = true;
                }
                else
                {
                    long const nEvents = atol(number.c_str());
                    
                    if (nEvents <= 0)
                    {
                        log << error << "Setting \"" << stgNumEvents.getPath() << "[" << i <<
                         "]\" makes the training set empty. The number might be misformated." <<
                         eom;
                    }
                    
                    if (not simpleNumberFound or nEvents < long(sample.maxTrainEvents))
                        sample.maxTrainEvents = nEvents;
                    
                    simpleNumberFound = true;
                }
            }
            
            if (not simpleNumberFound)
                sample.maxTrainEvents = -1;
            
            if (not fractionFound)
                sample.maxFractionTrainEvents = 1.;
            
            sample.trainEventsFileName = "";
        }
        else
        // The user specified neither a file containing the event list nor a number of events to
        //be used for training
        {
            // Default values
            sample.maxTrainEvents = -1;
            sample.maxFractionTrainEvents = 0.5;
            sample.trainEventsFileName = "";
        }
        
        
        // Push the sample into the vector
        samples.push_back(sample);
    }
}


string const & Config::GetTaskName() const
{
    return taskName;
}


vector<Config::Sample> const & Config::GetSamples() const
{
    return samples;
}


vector<string> const & Config::GetVariables() const
{
    return variables;
}


vector<Config::InputTransformation> const & Config::GetTransformations() const
{
    return inputTransformations;
}


string const & Config::GetFBMPath() const
{
    return FBMPath;
}


bool Config::GetKeepTempFiles() const
{
    return keepTempFiles;
}


string const & Config::GetBNNFileName() const
{
    return networkName;
}


Config::Reweighting Config::GetReweightingType() const
{
    return reweightingType;
}


unsigned Config::GetBNNNumberNeurons() const
{
    return numberNeurons;
}


string const & Config::GetBNNHyperparameters() const
{
    return networkHyperparameters;
}


string const & Config::GetBNNGenerationParameters() const
{
    return networkGenerationParameters;
}


std::pair<string const &, string const &> Config::GetBNNMCMCParameters() const
{
    return std::pair<string const &, string const &>(MCMCParametersFirstIt, MCMCParameters);
}


unsigned Config::GetBNNMCMCIterations() const
{
    return numberIterations;
}


unsigned Config::GetBNNMCMCBurnIn() const
{
    return burnInIterations;
}


string const & Config::GetCPPFileName() const
{
    return networkCPPFileName;
}
