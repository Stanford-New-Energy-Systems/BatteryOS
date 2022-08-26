#ifndef UTIL_HPP
#define UTIL_HPP
#include <sstream>
#include <iostream>

#define LOG() \
    LogStream(__func__, __FILE__, __LINE__).writeToLog()

#define WARNING() \
    WarningStream(__func__, __FILE__, __LINE__).writeWarning()

#define ERROR() \
    ErrorStream(__func__, __FILE__, __LINE__).writeError()

#define PRINT() \
    PrintStream().printMsg()

class LogStream {
    private:
        std::stringstream msg;

    public:
        LogStream(const char* function, const char* filename, int line) {
            this->msg << "LOG... " << filename << ":" << line << ", " << function << ": ";
        }

        ~LogStream() {
            std::cout << msg.str();
        }

        std::stringstream& writeToLog() {
            return this->msg;
        }
};

class WarningStream {
    private:
        std::stringstream msg;

    public:
        WarningStream(const char* function, const char* filename, int line) {
            this->msg << "---------------------------WARNING------------------------------" << std::endl;
            this->msg << filename << ":" << line << ", " << function << ": ";
        }

        ~WarningStream() {
            this->msg << "-------------------------END WARNING----------------------------" << std::endl;
            std::cerr << msg.str();
        }

        std::stringstream& writeWarning() {
            return this->msg;
        }
};

class ErrorStream {
    private:
        std::stringstream msg;

    public:
        ErrorStream(const char* function, const char* filename, int line) {
            this->msg << "---------------------------ERROR------------------------------" << std::endl;
            this->msg << filename << ":" << line << ", " << function << ": ";
        }

        ~ErrorStream() {
            this->msg << "-------------------------END ERROR----------------------------" << std::endl;
            this->msg << "Exiting with code 1 ... " << std::endl;
            std::cerr << msg.str();
            exit(1);
        }

        std::stringstream& writeError() {
            return this->msg;
        }
};

class PrintStream {
    private:
        std::stringstream msg;

    public:
        PrintStream() = default;

        ~PrintStream() {
            std::cout << msg.str();
        }

        std::stringstream& printMsg() {
            return this->msg;
        }
};

#endif
