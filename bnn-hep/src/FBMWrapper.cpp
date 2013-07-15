#include "FBMWrapper.hpp"
#include "utility.hpp"

#include <sstream>
#include <cstdlib>
#include <stdio.h>
#include <boost/algorithm/string.hpp>


using namespace logger;
using namespace std;


FBMWrapper::FBMWrapper(Logger &log_, Config const &config_, InputProcessor const &inputProcessor_):
    log(log_), config(config_), inputProcessor(inputProcessor_),
    FBMPath(config.GetFBMPath()), BNNFileName(config.GetBNNFileName())
{
    log << info(1) << "Training started. FBM binary file: \"" << config.GetBNNFileName() << "\"." <<
     eom;
    
    // Save the neural network architecture for future use
    NNArchitecture.reserve(3);
    NNArchitecture.push_back(inputProcessor.GetDim());
    NNArchitecture.push_back(config.GetBNNNumberNeurons());
    NNArchitecture.push_back(1);
    
    // Perform training
    TrainBNN();
    
    log << info(1) << "Training is completed." << eom;
}


FBMWrapper::~FBMWrapper()
{
    if (not config.GetKeepTempFiles())
    {
        remove(BNNFileName.c_str());
        log << info(2) << "Temporary file \"" << BNNFileName << "\" removed." << eom;
    }
}


void FBMWrapper::TrainBNN() const
{
    ostringstream command;  // stream to keep system commands
    string const &trainFileName = inputProcessor.GetTrainFileName();
    
    int exitCode;
    
    
    // Define the network
    command << FBMPath << "net-spec " << BNNFileName << " " << inputProcessor.GetDim() << " " <<
     config.GetBNNNumberNeurons() << " 1 / " << config.GetBNNHyperparameters();
    exitCode = system(command.str().c_str());
    
    if (exitCode != 0)
    {
        log << critical << "\"" << command.str() << "\" terminated with an error." << eom;
        exit(1);
    }
    
    // Reset the random seed
    command.str("");
    command << FBMPath << "rand-seed " << BNNFileName << " " << RandomInt(32767);
    exitCode = system(command.str().c_str());
    
    if (exitCode != 0)
    {
        log << critical << "\"" << command.str() << "\" terminated with an error." << eom;
        exit(1);
    }
    
    // Define the model
    command.str("");
    command << FBMPath << "model-spec " << BNNFileName << " binary";
    exitCode = system(command.str().c_str());
    
    if (exitCode != 0)
    {
        log << critical << "\"" << command.str() << "\" terminated with an error." << eom;
        exit(1);
    }
    
    // Define the training data
    command.str("");
    command << FBMPath << "data-spec " << BNNFileName << " " << inputProcessor.GetDim() <<
     " 1 2 / " << trainFileName << ":/Vars";
    
    for (unsigned i = 0; i < inputProcessor.GetDim(); ++i)
        // First two branches contain targets and weights, branch indices start from 1
        command << "," << i + 3;
    
    // Targets (they are written in the first branch)
    command << " " << trainFileName << ":/Vars,1";
    
    // Weights (they are written in the second branch, they are tacken as is)
    command << " weights=" << trainFileName << ":/Vars,2 rescale_weights=0";
    
    // Redirect the output not the litter the log
    command << " &> /dev/null";
    
    // No transformation of the variables is specified in data-spec, i.e. they are tacken as is
    exitCode = system(command.str().c_str());
    
    if (exitCode != 0)
    {
        log << critical << "\"" << command.str() << "\" terminated with an error." << eom;
        exit(1);
    }
    
    // Generate the initial neural network
    command.str("");
    command << FBMPath << "net-gen " << BNNFileName << " " << config.GetBNNGenerationParameters();
    exitCode = system(command.str().c_str());
    
    if (exitCode != 0)
    {
        log << critical << "\"" << command.str() << "\" terminated with an error." << eom;
        exit(1);
    }
    
    auto MCMCParams = config.GetBNNMCMCParameters();
    
    // Treat the first training iteration in a special way
    command.str("");
    command << FBMPath << "mc-spec " << BNNFileName << " " << MCMCParams.first << "; ";
    command << FBMPath << "net-mc " << BNNFileName << " 1";
    exitCode = system(command.str().c_str());
    
    if (exitCode != 0)
    {
        log << critical << "\"" << command.str() << "\" terminated with an error." << eom;
        exit(1);
    }
    
    // Perform the training
    command.str("");
    command << FBMPath << "mc-spec " << BNNFileName << " " << MCMCParams.second << "; ";
    command << FBMPath << "net-mc " << BNNFileName << " " << config.GetBNNMCMCIterations();
    exitCode = system(command.str().c_str());
    
    if (exitCode != 0)
    {
        log << critical << "\"" << command.str() << "\" terminated with an error." << eom;
        exit(1);
    }
}


void FBMWrapper::ErrorWrongOutput(string const &command) const
{
    log << error << "The output of \"" << command << "\" is badly formatted." << eom;
    exit(1);
}


NeuralNetwork FBMWrapper::ReadNN(unsigned index) const
{
    ostringstream command;
    stringstream output;  // this will store the output of the command
    string line;
    
    
    // Read the NN with the given index
    command << FBMPath << "net-display -p " << BNNFileName << " " << index;
    
    // Execute the command
    FILE *pipe = popen(command.str().c_str(), "r");
    
    if (!pipe)
    {
        log << error << "\"" << command.str() << "\" terminated with an error." << eom;
        exit(1);
    }
    
    // Read the command's output
    char buffer[128];
    
    while (fgets(buffer, 128, pipe))
        output << buffer;
    
    pclose(pipe);
    
    
    // Create a neural network
    NeuralNetwork nn(NNArchitecture);
    
    
    // Parse the output
    // First, skip the header
    getline(output, line);
    getline(output, line);
    getline(output, line);
    
    // Loop over the layers (all but one)
    for (unsigned l = 1; l < NNArchitecture.size(); ++l)
    {
        getline(output, line);
        
        if (line.find("Weights") == string::npos)
            ErrorWrongOutput(command.str());
        
        getline(output, line);  // skip an empty line
        
        // Loop over the groups corresponding to the nodes in the previous layer
        for (unsigned np = 0; np < NNArchitecture.at(l - 1); ++np)
        {
            // Loop over the current layer
            for (unsigned n = 0; n < NNArchitecture.at(l); ++n)
            {
                double w;
                output >> w;
                
                nn.GetWeight(l, n, np) = w;
            }
            
            getline(output, line);  // skip an empty line
        }
        
        getline(output, line);  // skip an empty line
        getline(output, line);
        
        if (line.find("Biases") == string::npos)
            ErrorWrongOutput(command.str());
        
        getline(output, line);  // skip an empty line
        
        // Loop over the nodes in the current layer
        for (unsigned n = 0; n < NNArchitecture.at(l); ++n)
        {
            double b;
            output >> b;
            
            nn.GetBias(l, n) = b;
        }
        
        getline(output, line);  // skip an empty line
        getline(output, line);  // skip an empty line
    }
    
    
    return nn;
}
