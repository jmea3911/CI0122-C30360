#include "syscall.h"

void hijo(int);
int id;
int contador;		// shared by father and child

int main() {
	id = SemCreate(0);
	Fork(hijo);

	SemWait(id);		// waits until the child is done
	Write("padre\n", 6, 1);
	Write((char *) contador, 0, 2);		// ConsoleError prints an integer
	SemDestroy(id);
	Exit(0);
}

void hijo(int dummy) {
	contador = 42;
	Write("hijo\n", 5, 1);
	SemSignal(id);
}