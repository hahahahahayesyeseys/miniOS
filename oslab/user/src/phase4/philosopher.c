#include "philosopher.h"

// TODO: define some sem if you need
int chop[PHI_NUM];
int mutex;
void init() {
  // init some sem if you need
 mutex=sem_open(1);
 for(int i=0;i<PHI_NUM;++i){
  chop[i]=sem_open(1);
 }
}

void philosopher(int id) {
  // implement philosopher, remember to call `eat` and `think`
   
  while (1) {
 
    P(mutex);
    P(chop[id]);
    P(chop[(id+1)%5]);
    V(mutex);
    eat(id);
    V(chop[id]);
    V(chop[(id+1)%5]);
    think(id);
    
  }
}
