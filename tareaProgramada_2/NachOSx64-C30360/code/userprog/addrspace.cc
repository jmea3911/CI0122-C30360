// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
// Copyright (c) -2026 Universidad de Costa Rica

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#include "bitmap.h"
#include "synch.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

        DEBUG('a', "Initializing address space, num pages %d, size %d\n",
                    numPages, size);



// check there are enough free frames, then take them (all under the mutex)
    if (numPages > (unsigned int)machine->memoryMap->NumClear()) {
        machine->memoryMapSem->V();
        // code and data pages can be shared by Fork; the stack is always private
        
        printf("Not enough free frames: need %d, have %d\n", numPages,
                machine->memoryMap->NumClear());
        ASSERT(false);
    }

    this->pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = machine->memoryMap->Find();
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
        DEBUG('a', "Virtual page %d -> physical frame %d\n", i,
                pageTable[i].physicalPage);
    }
    machine->memoryMapSem->V();
    sharedPages = numPages - UserStackSize / PageSize;
    sharedUsers = new int;
    *sharedUsers = 1;

// zero out ONLY our own frames (uninitialized data and stack included)
    for (i = 0; i < numPages; i++)
        bzero(&(machine->mainMemory[pageTable[i].physicalPage * PageSize]),
                PageSize);

// then, copy in the code and data segments, page by page
    if (noffH.code.size > 0) {
        LoadSegment(executable, noffH.code.virtualAddr, noffH.code.size,
                    noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        LoadSegment(executable, noffH.initData.virtualAddr,
                    noffH.initData.size, noffH.initData.inFileAddr);
    }
}

AddrSpace::AddrSpace(AddrSpace *parent)
{
    unsigned int i;

    numPages = parent->numPages;
    sharedPages = parent->sharedPages;
    sharedUsers = parent->sharedUsers;

    machine->memoryMapSem->P();
    if (numPages - sharedPages > (unsigned int)machine->memoryMap->NumClear()) {
        machine->memoryMapSem->V();
        printf("Not enough free frames for a new stack\n");
        ASSERT(false);
    }
    (*sharedUsers)++;

    this->pageTable = new TranslationEntry[numPages];
    for (i = 0; i < sharedPages; i++)
        pageTable[i] = parent->pageTable[i];		// shared frames
    for (i = sharedPages; i < numPages; i++) {		// private stack
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = machine->memoryMap->Find();
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
        DEBUG('a', "Fork: stack page %d -> physical frame %d\n", i,
                pageTable[i].physicalPage);
    }
    machine->memoryMapSem->V();

    for (i = sharedPages; i < numPages; i++)
        bzero(&(machine->mainMemory[pageTable[i].physicalPage * PageSize]),
                PageSize);
}

void
AddrSpace::LoadSegment(OpenFile *executable, int virtualAddr, int size,
                       int inFileAddr)
{
    int done = 0;

    while (done < size) {
        int vaddr = virtualAddr + done;
        int vpage = vaddr / PageSize;
        int offset = vaddr % PageSize;
        int chunk = PageSize - offset;      // up to the end of this page
        if (chunk > size - done)
            chunk = size - done;            // last, partial chunk

        int physAddr = pageTable[vpage].physicalPage * PageSize + offset;
        executable->ReadAt(&(machine->mainMemory[physAddr]), chunk,
                           inFileAddr + done);
        done += chunk;
    }
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    // machine is NULL if NachOS is already cleaning up (see Cleanup)
    if (machine != NULL) {
        machine->memoryMapSem->P();
        for (unsigned int i = sharedPages; i < numPages; i++)	// stack
            machine->memoryMap->Clear(pageTable[i].physicalPage);
        (*sharedUsers)--;
        if (*sharedUsers == 0) {		// last one: code and data too
            for (unsigned int i = 0; i < sharedPages; i++)
                machine->memoryMap->Clear(pageTable[i].physicalPage);
            delete sharedUsers;
        }
        DEBUG('a', "Address space freed, free frames now: %d\n",
                machine->memoryMap->NumClear());
        machine->memoryMapSem->V();
    }
    delete [] this->pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
