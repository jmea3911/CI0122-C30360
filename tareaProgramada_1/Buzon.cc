/**
  *   C++ class to encapsulate Unix message passing intrinsic structures and system calls
  *
  *   UCR-ECCI
  *
  *   CI0122 Sistemas Operativos 2026-ii
  *
  *   Class implementation 
 **/

#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "Buzon.h"

#define MAX_PAYLOAD 512

/* Estructura interna "sobre el cable": mtype + los bytes reales del mensaje. */
struct MsgWire {
   long mtype;
   char data[ MAX_PAYLOAD ];
};


/**
  *  Class constructor
  *
 **/
Buzon::Buzon() {
   int st = -1;

   // Use msgget to create a new mailbox
   st = msgget( KEY, IPC_CREAT | 0600 );

   if ( -1 == st ) {
      throw std::runtime_error( "Buzon::Buzon( int )" );
   }

   id = st;
   owner = getpid();
}


/**
  * Class destructor
  *
 **/
Buzon::~Buzon() {
   int st = 0;

   // Use msgctl to destroy mailbox (solo el proceso creador la elimina)
   if ( owner == getpid() ) {
      st = msgctl( id, IPC_RMID, NULL );
   }

   if ( -1 == st ) {
      throw std::runtime_error( "Buzon::~Buzon( int )" );
   }
}


/**
  *  Send method
  *
  *  @param     const char * mensaje: arreglo de caracteres a enviar
  *
 **/
int Buzon::Enviar( const char * mensaje, long tipo ) {
   return Enviar( (const void *) mensaje, (int)(strlen( mensaje ) + 1), tipo );
}


/**
  *  Send method
  *
  *  @param     const void * mensaje: estructura con el mensaje a enviar
  *  @param	int cantidad: cantidad de bytes a enviar
  *
  *
 **/
int Buzon::Enviar( const void * mensaje, int cantidad, long tipo ) {
   int st = -1;
   MsgWire buf;

   if ( cantidad < 0 || (size_t)cantidad > sizeof( buf.data ) ) {
      throw std::runtime_error( "Buzon::Enviar( const void *, int, long )" );
   }

   buf.mtype = tipo;
   memcpy( buf.data, mensaje, cantidad );

   // Use msgsnd to send the message
   st = msgsnd( id, &buf, cantidad, 0 );

   if ( -1 == st ) {
      throw std::runtime_error( "Buzon::Enviar( const void *, int, long )" );
   }

   return st;
}


/**
  *  Receive method
  *
  *  @param     const void * mensaje: estructura con el mensaje a enviar
  *  @param	int cantidad: cantidad de bytes a enviar
  *
  *
 **/
int Buzon::Recibir( void * mensaje, int cantidad, long tipo ) {
   int st = -1;
   MsgWire buf;

   if ( cantidad < 0 || (size_t)cantidad > sizeof( buf.data ) ) {
      throw std::runtime_error( "Buzon::Recibir( void *, int, long )" );
   }

   // Use msgrcv to receive the message
   st = msgrcv( id, &buf, cantidad, tipo, 0 );

   if ( -1 == st ) {
      throw std::runtime_error( "Buzon::Recibir( void *, int, long )" );
   }

   memcpy( mensaje, buf.data, st );

   return st;
}
