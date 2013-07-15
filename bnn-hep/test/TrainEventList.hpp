/**
 * \author Andrey Popov
 * 
 * This file describes a class to interface the text files with the lists of events tried for
 * training. It is intended for use with ROOT.
 */

#ifndef TRAIN_EVENT_LIST_HPP
#define TRAIN_EVENT_LIST_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>


/// Class to read the files with the lists of events tried for training
class TrainEventList
{
    public:
        /// Constuctor. Takes the name of the text file as the argument
        TrainEventList(std::string const &fileName);
    
    public:
        /**
         * \brief Read the event list.
         * 
         * Reads the list of the events tried for training.
         *  \param sampleFileName The name of the corresponding ROOT file used to identify the list.
         * It may contain some directories which are stripped off before the search.
         *  \return Returns true if the list was read successfully and false otherwise.
         */
        bool ReadEventList(std::string const &sampleFileName);
        
        /**
         * \brief Returns the number of events in the list last read.
         * 
         * Returns the length of the event list that was recently read from the text file. Returns
         * 0 in case the requested file name was not found in the text file.
         * \warning This information must be used to rescale the event weights in order to account
         * the fact that some of the events are excluded from the exam set.
         */
        unsigned long GetNEvents() const;
        
        /**
         * \brief Checks whether the given event can be used in the exam set.
         * 
         * Returns false if the given event has been tried for training and true otherwise. The
         * event is identified by its index number in the tree. In case the result of ReadEvenList
         * was false, this method always returns true (it is assumed that the whole file was not
         * tried for training).
         */
        bool CheckEventExam(unsigned long event) const;
    
    private:
        std::ifstream file;  ///< Stream associated with the file
        bool listRead;  ///< Indicates whether the list of events has been read
        unsigned long nEventsRead;  ///< Number of events in the list read
        std::vector<unsigned long> eventsRead;  ///< The list of events read
};


TrainEventList::TrainEventList(std::string const &fileName):
    file(fileName.c_str()),
    listRead(false), nEventsRead(0)
{}


bool TrainEventList::ReadEventList(std::string const &sampleFileName)
{
    listRead = false;
    
    // Set the get pointer to the beginning of the file
    file.seekg (0, std::ios::beg);
    
    // Find the line that contains the sample file name
    std::string const shortFileName = sampleFileName.substr(sampleFileName.find_last_of('/') + 1);
    std::string line;
    
    while (true)
    {
        std::getline(file, line);
        
        if (line.find(shortFileName) != std::string::npos)
            break;
        
        if (file.eof() or file.fail())
            return false;
    }
    
    // Skip two lines (an empty one and a comment)
    std::getline(file, line);
    std::getline(file, line);
    
    // Read the number of events
    file >> nEventsRead;
    
    // Skip three lines (the ending of the current one, an empty line and a comment)
    for (unsigned i = 0; i < 3; ++i)
        std::getline(file, line);
    
    // Read the list of events
    eventsRead.reserve(nEventsRead);
    unsigned long ev;
    
    for (unsigned long i = 0; i < nEventsRead; ++i)
    {
        file >> ev;
        eventsRead.push_back(ev);
    }
    
    
    // All done
    listRead = true;
    return true;
}


unsigned long TrainEventList::GetNEvents() const
{
    if (not listRead)
        //std::cerr << "Error. The requested train event list was not found.\n";
        return 0;
    
    return nEventsRead;
}


bool TrainEventList::CheckEventExam(unsigned long event) const
{
    if (not listRead)
        //std::cerr << "Error. The requested train event list was not found.\n";
        return true;
    
    return not std::binary_search(eventsRead.begin(), eventsRead.end(), event);
}


#endif
