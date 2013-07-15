/**
 * \author Andrey Popov
 * 
 * The module is responsible for splitting the available cases of expert classification into the
 * training and exam sets, performing the preprocessing, and preparing the input file for FBM.
 */

#pragma once

#include "Logger.hpp"
#include "Config.hpp"
#include "TransformBase.hpp"

#include <Rtypes.h>
#include <TTreeFormula.h>

#include <list>


using namespace logger;
using std::list;


/**
 * \brief The class prepares the training file for FBM.
 * 
 * The class splits the available set of cases of expert classification into the training and exam
 * sets. It produces a text file that lists the events that have been tried for the training set and
 * therefore must not enter the exam set. It performs the required preprocessing of the input
 * variables. Finally it applies the preprocessing and prepare the input file for FBM to be used for
 * training.
 */
class InputProcessor
{
    private:
        /// Structure to present an event in the training set
        struct Event
        {
            /// Static number of input variables (dimensionality)
            static unsigned nVars;
            
            /// Default constructor
            Event():
                type(-1), weight(-1.), vars(nullptr)
            {}
            
            /// Constructor with parameters
            Event(UInt_t type_, Double_t weight_, vector<Double_t> const &vars_):
                type(type_), weight(weight_)
            {
                //nVars = vars_.size();
                vars = new Double_t[nVars];
                
                for (unsigned i = 0; i < nVars; ++i)
                    vars[i] = vars_.at(i);
            }
            
            /// Constructor from TTreeFormula's
            Event(UInt_t type_, Double_t weight_, vector<TTreeFormula *> const &vars_):
                type(type_), weight(weight_)
            {
                //nVars = vars_.size();
                vars = new Double_t[nVars];
                
                for (unsigned i = 0; i < nVars; ++i)
                    vars[i] = vars_.at(i)->EvalInstance();
            }
            
            // Do I need the move constructor and move assignment?
            
            /// Destructor
            ~Event()
            {
                delete [] vars;
                vars = nullptr;
            }
            
            UInt_t type;      ///< Classification type (currently signal or background)
            Double_t weight;  ///< Weight of the event
            Double_t *vars;   ///< Input variables
        };
    
    public:
        /**
         * \brief Constructor.
         * 
         * Constructor. It takes the Logger and Config objects as arguements. All the actions
         * performed by the class are called from here.
         */
        InputProcessor(Logger &log_, Config const &config_);
        
        
        /// Destructor
        ~InputProcessor();
        
        /// Copy constructor (not allowed to be used)
        InputProcessor(InputProcessor const &) = delete;
        
        /// Assignment operator (not allowed to be used)
        InputProcessor const & operator=(InputProcessor const &) = delete;
    
    private:
        /// Chooses the events to be used for training. Writes their ID in a text file
        void BuildTrainingSet();
        
        /// Builds and applies the transformation to the input variables
        void TransformInputs();
        
        /// Writes ROOT file containing the tree for training
        void WriteTrainFile() const;
    
    public:
        /// Returns the number of input variables
        unsigned GetDim() const;
        
        /// Returns the name of the ROOT training file
        string const & GetTrainFileName() const;
        
        /// Returns the list of the transformations
        list<TransformBase *> const & GetTransformations() const;
    
    private:
        Logger &log;  ///< Logger instance
        Config const &config;  ///< Config instance
        list<Event> trainingSet;  ///< Training set
        list<TransformBase *> transforms;  ///< Transformations of input variables
        string const trainingFileName;  ///< Name of the ROOT file used as input for FBM
};
