#ifndef NACHOSTABLA_H
#define NACHOSTABLA_H

#include "bitmap.h"

#define MAX_OPEN_FILES 32

class NachosOpenFilesTable {
  public:
    NachosOpenFilesTable();
    ~NachosOpenFilesTable();

    int Open( int UnixHandle );        // returns the NachOS handle, -1 if full
    int Close( int NachosHandle );     // 0 or -1
    bool isOpened( int NachosHandle );
    int getUnixHandle( int NachosHandle );

    void addThread();                  // a thread starts using this table
    int delThread();                   // returns how many threads still use it

    void Print();

  private:
    int * openFiles;                   // NachOS handle -> Unix handle
    BitMap * openFilesMap;             // which NachOS handles are in use
    int usage;                         // how many threads use this table
};

#endif