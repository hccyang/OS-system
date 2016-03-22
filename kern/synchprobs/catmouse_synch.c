#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <spl.h>


/* 
 * This simple default synchronization mechanism allows only creature at a time to
 * eat.   The globalCatMouseSem is used as a a lock.   We use a semaphore
 * rather than a lock so that this code will work even before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 //howe variables
 */
 
 //keeps track if theres something eating there or not
static char * foodbowls;

//make N locks for N bowls;
static struct lock ** thelock;

static struct lock * globallock;

static int bowl_num;

static volatile int cats_eating;

static volatile int mice_eating;

static int currentlyeating;
// currentlyeating = 1 if cats are eating
//                 = 2 if mouse are eating

static struct cv * cv_eating;

static struct cv * cv_sleeping;
 
 
//static struct semaphore *globalCatMouseSem;


/* 
 * The CatMouse simulation will call this function once before any cat or
 * mouse tries to each.
 *
 * You can use it to initialize synchronization and other variables.
 * 
 * parameters: the number of bowls
 */
void
catmouse_sync_init(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_init */
  
  cv_eating = cv_create("eating");
  if(cv_eating == NULL){
   panic("WHAATT THE MOLLY");
  }
  
  cv_sleeping = cv_create("sleeping");
  if(cv_sleeping == NULL){
   panic("WHAATT THE MOLLY");
  }
  
  int counter;
  
  if(bowls == 0){
  //return 1;
  }
  
  foodbowls = kmalloc(bowls * sizeof(char));
  
  thelock = kmalloc(bowls * sizeof(struct lock));
  
  //initialize array
  
  for(counter = 0; counter < bowls; counter ++){
  //make it empty
  foodbowls[counter] = 'x';
  thelock[counter] = lock_create("lock");
  }
  
  globallock = lock_create("global");
  
  cats_eating = 0;
  
  mice_eating = 0;
  
  currentlyeating = 0;
  
  bowl_num = bowls;
  
  
  return;
}

/* 
 * The CatMouse simulation will call this function once after all cat
 * and mouse simulations are finished.
 *
 * You can use it to clean up any synchronization and other variables.
 *
 * parameters: the number of bowls
 */
void
catmouse_sync_cleanup(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_cleanup */
  (void)bowls; /* keep the compiler from complaining about unused parameters */

  
  
  KASSERT(thelock != NULL);
  
  int counter = 0;
  
  for(counter = 0; counter < bowls; counter ++){

  lock_destroy(thelock[counter]);
  }
  kfree(thelock);
  //should i set it to null
  
  lock_destroy(globallock);
  
  cv_destroy(cv_eating);
  cv_destroy(cv_sleeping);
  
  //int buffer = bowls;
  kfree(foodbowls);
}


/*
 * The CatMouse simulation will call this function each time a cat wants
 * to eat, before it eats.
 * This function should cause the calling thread (a cat simulation thread)
 * to block until it is OK for a cat to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the cat is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_before_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of cat_before_eating */
  //(void)bowl;  /* keep the compiler from complaining about an unused parameter */
  //KASSERT(globalCatMouseSem != NULL);
  //P(globalCatMouseSem);
    
 // kprintf("a cat is prepping for bowl: %d \n", bowl-1);
  
  KASSERT(globallock != NULL);
  KASSERT(thelock[bowl-1] != NULL);
  
  
  lock_acquire(thelock[bowl-1]);
  cv_broadcast(cv_eating, thelock[bowl-1]);

  //CV WAITTT
  while(foodbowls[bowl-1] != 'x'  || mice_eating > 0){
  
   //kprintf("currently eating: %d  foodbowl: %c   cats eating: %d\n", currentlyeating, foodbowls[bowl-1], cats_eating);
   
   // kprintf("b");
    
      if(mice_eating > 0){
  for(int counter = 0; counter < bowl_num; counter++){
  lock_acquire(thelock[counter]);
  cv_signal(cv_eating,thelock[counter]);
  lock_release(thelock[counter]);
  }
   lock_acquire(thelock[bowl-1]);
  }
  
   
  cv_wait(cv_sleeping, thelock[bowl-1]);
  }
  lock_acquire(thelock[bowl-1]);
  
   //kprintf("a cat is gonna eat at bowl: %d \n", bowl-1);
   /*kprintf("number of cats: %d number of mice: %d \n", cats_eating, mice_eating);*/
  lock_acquire(globallock);
  KASSERT(foodbowls[bowl-1] == 'x');
  KASSERT(mice_eating == 0);
  
  cats_eating = cats_eating + 1;
  foodbowls[bowl-1] = 'C';
  currentlyeating = 1;
  
  return;
  
  
}

/*
 * The CatMouse simulation will call this function each time a cat finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this cat finished.
 *
 * parameter: the number of the bowl at which the cat is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_after_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of cat_after_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */

  //kprintf("a cat has finished eating\n");
// might need to grab lock again
   // lock_acquire(thelock[bowl-1]);
  KASSERT(cats_eating > 0);
  KASSERT(foodbowls[bowl-1] == 'C');
  
  cats_eating = cats_eating -1;
  foodbowls[bowl-1] = 'x';

  
 lock_release(globallock);
  cv_signal(cv_eating, thelock[bowl-1]);
  cv_broadcast(cv_sleeping, thelock[bowl-1]);
  lock_release(thelock[bowl-1]);
  
  return;
}

/*
 * The CatMouse simulation will call this function each time a mouse wants
 * to eat, before it eats.
 * This function should cause the calling thread (a mouse simulation thread)
 * to block until it is OK for a mouse to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the mouse is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_before_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of mouse_before_eating */
  //(void)bowl;  /* keep the compiler from complaining about an unused parameter */

 // kprintf("a mouse is prepping eat at bowl: %d \n", bowl-1);


 
 lock_acquire(thelock[bowl-1]);
 cv_broadcast(cv_eating, thelock[bowl-1]);

 while( foodbowls[bowl-1] != 'x' || cats_eating > 0){ 
  //kprintf("currently eating: %d  foodbowl: %c   cats eating: %d\n", currentlyeating, foodbowls[bowl-1], cats_eating);
   //kprintf("a");
  if(cats_eating > 0){
  for(int counter = 0; counter < bowl_num; counter++){
  lock_acquire(thelock[counter]);
  cv_signal(cv_sleeping,thelock[counter]);
  lock_release(thelock[counter]);
  }
  lock_acquire(thelock[bowl-1]);
  }
  
  cv_wait(cv_sleeping, thelock[bowl-1]);
   
  
  }
  
 
   // kprintf("a mouse is gonna eat at bowl: %d \n", bowl-1);

lock_acquire(globallock);
KASSERT(foodbowls[bowl -1] == 'x');
KASSERT(cats_eating == 0);

mice_eating = mice_eating + 1;


foodbowls[bowl -1] = 'M';

currentlyeating = 2;


return;
 
 
}

/*
 * The CatMouse simulation will call this function each time a mouse finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this mouse finished.
 *
 * parameter: the number of the bowl at which the mouse is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_after_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of mouse_after_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */

  // kprintf("a mouse has finished eating\n");
    //lock_acquire(thelock[bowl-1]);
KASSERT(mice_eating > 0);
mice_eating = mice_eating - 1;
foodbowls[bowl-1] = 'x';


lock_release(globallock);
 cv_signal(cv_sleeping, thelock[bowl-1]);
 cv_broadcast(cv_eating, thelock[bowl-1]);
 lock_release(thelock[bowl-1]);
 return;
}
