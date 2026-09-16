/**
  *   CI-0122 Sistemas Operativos - Tarea Programada I
  *   "Juego de la papa caliente" - anillo de procesos, usando la clase Buzon
  *
  *   Uso:  ./papa n v [H|A]
  *     n   -> cantidad de participantes (procesos en el anillo)
  *     v   -> valor inicial de la papa
  *     H|A -> sentido: H horario (defecto, i -> i+1, n-1 -> 0)
  *                     A antihorario (i -> i-1, 0 -> n-1)
 **/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "Buzon.h"

#define RANGO_VALOR_MAX 500
#define KEY_SHM  (KEY + 1)   // llave para la memoria compartida (contador de activos)
#define KEY_SEM  (KEY + 2)   // llave para el semaforo (mutex del contador)

struct Papa {
   long valor;
};

// Helpers para el semaforo System V 
static void sem_P( int semid ) {
   struct sembuf op = { 0, -1, 0 };
   semop( semid, &op, 1 );
}
static void sem_V( int semid ) {
   struct sembuf op = { 0, +1, 0 };
   semop( semid, &op, 1 );
}

static long collatz_paso( long v ) {
   if ( v % 2 == 0 ) return v / 2;
   return 3 * v + 1;
}

// Codigo que ejecuta cada proceso participante del anillo
static void proceso_participante( int id, int siguiente, Buzon &m,
                                    int *activos, int semid ) {
   long tipo_propio    = id + 1;         // los tipos deben ser > 0
   long tipo_siguiente = siguiente + 1;

   srand( (unsigned int)( time(NULL) ^ (getpid() << 16) ^ id ) );

   int activo = 1;

   for ( ;; ) {
      Papa msg;
      m.Recibir( (void *)&msg, sizeof(msg), tipo_propio );

      if ( msg.valor < 0 ) {
         // Mensaje de fin de juego: -(id_ganador + 1) 
         int id_ganador = (int)( -msg.valor - 1 );
         if ( id_ganador == id ) {
            break;   // mi propio aviso me dio la vuelta completa al anillo
         }
         Papa reenvio = msg;
         m.Enviar( (const void *)&reenvio, sizeof(reenvio), tipo_siguiente );
         break;
      }

      if ( activo ) {
         long resultado = collatz_paso( msg.valor );
         printf( "[Proceso %d] recibe papa=%ld -> Collatz=%ld\n", id, msg.valor, resultado );
         fflush( stdout );

         if ( resultado == 1 ) {
            activo = 0;

            sem_P( semid );
            (*activos)--;
            int restantes = *activos;
            sem_V( semid );

            long nuevo_valor = 1 + ( rand() % RANGO_VALOR_MAX );
            printf( "[Proceso %d] La papa exploto. Salgo del juego (pasivo). "
                    "Nuevo valor elegido: %ld. Quedan %d activo(s).\n",
                    id, nuevo_valor, restantes );
            fflush( stdout );

            if ( restantes == 0 ) {
               printf( "\n==> PROCESO GANADOR: %d (valor elegido al salir: %ld) <==\n\n",
                       id, nuevo_valor );
               fflush( stdout );

               Papa fin;
               fin.valor = -( (long)id + 1 );
               m.Enviar( (const void *)&fin, sizeof(fin), tipo_siguiente );
               break;
            } else {
               Papa out;
               out.valor = nuevo_valor;
               m.Enviar( (const void *)&out, sizeof(out), tipo_siguiente );
            }
         } else {
            Papa out;
            out.valor = resultado;
            m.Enviar( (const void *)&out, sizeof(out), tipo_siguiente );
         }
      } else {
         m.Enviar( (const void *)&msg, sizeof(msg), tipo_siguiente );
      }
   }

   _exit( 0 );
}

int main( int argc, char *argv[] ) {
   if ( argc < 3 ) {
      fprintf( stderr, "Uso: %s n v [H|A]\n", argv[0] );
      fprintf( stderr, "  n   cantidad de participantes\n" );
      fprintf( stderr, "  v   valor inicial de la papa\n" );
      fprintf( stderr, "  H|A sentido: H=horario (defecto), A=antihorario\n" );
      return 1;
   }

   int n = atoi( argv[1] );
   long v = atol( argv[2] );
   int horario = 1;
   if ( argc >= 4 && ( argv[3][0] == 'A' || argv[3][0] == 'a' ) ) {
      horario = 0;
   }

   if ( n < 1 ) {
      fprintf( stderr, "n debe ser >= 1\n" );
      return 1;
   }

   // Buzon: se crea UNA sola vez, antes del fork 
   Buzon m;   

   // elementos de sincronizacion
   int shmid = shmget( KEY_SHM, sizeof(int), IPC_CREAT | 0600 );
   if ( shmid == -1 ) { perror( "shmget" ); return 1; }
   int *activos = (int *) shmat( shmid, NULL, 0 );
   if ( activos == (void *) -1 ) { perror( "shmat" ); return 1; }
   *activos = n;

   int semid = semget( KEY_SEM, 1, IPC_CREAT | 0600 );
   if ( semid == -1 ) { perror( "semget" ); return 1; }
   if ( semctl( semid, 0, SETVAL, 1 ) == -1 ) { perror( "semctl SETVAL" ); return 1; }

   //fork() de los n procesos participantes 
   pid_t *pids = new pid_t[n];
   for ( int i = 0; i < n; i++ ) {
      int siguiente = horario ? ( i + 1 ) % n : ( i - 1 + n ) % n;
      pid_t pid = fork();
      if ( pid < 0 ) {
         perror( "fork" );
         return 1;
      } else if ( pid == 0 ) {
         delete[] pids;
         proceso_participante( i, siguiente, m, activos, semid );
         /* no retorna */
      } else {
         pids[i] = pid;
      }
   }

  
   srand( (unsigned int)( time(NULL) ^ getpid() ) );
   int inicial = rand() % n;
   long tipo_inicial = inicial + 1;

   printf( "[main] %d participantes, papa arranca en el proceso %d con valor %ld, sentido %s.\n",
           n, inicial, v, horario ? "horario" : "antihorario" );
   fflush( stdout );

   Papa msg_inicial;
   msg_inicial.valor = v;
   m.Enviar( (const void *)&msg_inicial, sizeof(msg_inicial), tipo_inicial );

   //espera a que todos los participantes terminen. 
   for ( int i = 0; i < n; i++ ) {
      int status;
      waitpid( pids[i], &status, 0 );
   }
   delete[] pids;

   // Limpieza de los elementos de sincronizacion 
   shmdt( activos );
   shmctl( shmid, IPC_RMID, NULL );
   semctl( semid, 0, IPC_RMID );
   // El destructor de "m" (Buzon) elimina la cola de mensajes, ya que
   // este proceso (main) es el "owner" que la creo.

   printf( "[main] Juego terminado. Recursos liberados.\n" );
   return 0;
}
