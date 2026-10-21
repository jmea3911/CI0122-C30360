#include <unistd.h>
#include <stdio.h>
#include "nachostabla.h"

NachosOpenFilesTable::NachosOpenFilesTable()
{
    openFiles = new int[ MAX_OPEN_FILES ];
    openFilesMap = new BitMap( MAX_OPEN_FILES );
    usage = 1;                          // the thread that creates it

    for ( int i = 0; i < MAX_OPEN_FILES; i++ )
        openFiles[ i ] = -1;

    for ( int i = 0; i < 3; i++ ) {     // console: stdin, stdout, stderr
        openFilesMap->Mark( i );
        openFiles[ i ] = i;
    }
}

NachosOpenFilesTable::~NachosOpenFilesTable()
{
    for ( int i = 3; i < MAX_OPEN_FILES; i++ )   // never close the console
        if ( openFilesMap->Test( i ) )
            close( openFiles[ i ] );
    delete [] openFiles;
    delete openFilesMap;
}

int NachosOpenFilesTable::Open( int UnixHandle )
{
    int handle = openFilesMap->Find();  // finds a free handle and marks it
    if ( handle == -1 )
        return -1;
    openFiles[ handle ] = UnixHandle;
    return handle;
}

int NachosOpenFilesTable::Close( int NachosHandle )
{
    if ( NachosHandle < 3 || ! isOpened( NachosHandle ) )
        return -1;
    int result = close( openFiles[ NachosHandle ] );
    openFilesMap->Clear( NachosHandle );
    openFiles[ NachosHandle ] = -1;
    return result;
}

bool NachosOpenFilesTable::isOpened( int NachosHandle )
{
    if ( NachosHandle < 0 || NachosHandle >= MAX_OPEN_FILES )
        return false;
    return openFilesMap->Test( NachosHandle );
}

int NachosOpenFilesTable::getUnixHandle( int NachosHandle )
{
    if ( ! isOpened( NachosHandle ) )
        return -1;
    return openFiles[ NachosHandle ];
}

void NachosOpenFilesTable::addThread() { usage++; }

int NachosOpenFilesTable::delThread() { usage--; return usage; }

void NachosOpenFilesTable::Print()
{
    printf( "Open files table (used by %d threads):\n", usage );
    for ( int i = 0; i < MAX_OPEN_FILES; i++ )
        if ( openFilesMap->Test( i ) )
            printf( "  NachOS handle %d -> Unix handle %d\n", i, openFiles[ i ] );
}