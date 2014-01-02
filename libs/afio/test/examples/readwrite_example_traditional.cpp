#include "boost/afio/afio.hpp"
#include <iostream>
#include <fstream>
// Need to include a copy of ASIO
#ifdef BOOST_ASIO_SEPARATE_COMPILATION
#include "../../../../boost/asio/impl/src.hpp"
#endif

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) || (defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */)
    //[readwrite_example_traditional
        
    try
    {
        // Open a file called example_file.txt
        std::fstream openfile("example_file.txt"); /*< opens file >*/
        // Turn on exception throwing
        openfile.exceptions(std::fstream::failbit | std::fstream::badbit);
        
        // Do a write gather. STL iostreams will buffer the writes
        // and coalesce them into a single syscall
        openfile << "He";  /*< writes >*/
        openfile << "ll";
        openfile << "o ";
        openfile << "Wo";
        openfile << "rl";
        openfile << "d\n";
        
        // Make sure the previous batch has definitely reached physical storage
        // This won't complete until the write is on disc
        openfile.sync(); /*< syncs >*/
                
        // Fill this array from the file
        std::array<char, 12> buffer;
        openfile.seekg(0, std::ios::beg);
        openfile.read(buffer.data(), buffer.size());  /*< reads >*/
            
        // Close the file and delete it
        openfile.close();  /*< closes file >*/
        std::filesystem::remove("example_file.txt");  /*< deletes file >*/
        
        // Convert the read array into a string
        std::string contents(buffer.begin(), buffer.end());
        std::cout << "Contents of file is '" << contents << "'" << std::endl;
    }
    catch(...)
    {
        std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
        throw;
    }
    //]
#endif
}
