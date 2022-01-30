// File Name: csmc.c
// Authors: Jagan Mohan Reddy Bijjam (JXM210003)
// Description: Solving the seeking tutor problem using concurrent threads for OS Project 3



// Including the necessary libraries
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
// #include <stdint.h>
#include <assert.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h> 
#include <time.h>

// This function is used for debugging purpose
void cprintf(char* output){
	bool debugging = false;
	if(debugging){
		printf("%s \n", output);
	}
}

// The fun begins now...


// *************** Struct declarations *******************


// Defining the student struct
struct studentClass {
	int studentId;			// The id of the student
	int numOfHelpsTaken;	// Number of times he has taken help from the tutor
	int tutorIdHelped;		// The id of the tutor who helped him
	int tutoringComplete;	// This variable will be used for student thread spin wait as a semophore might unlock some other student
};

// Defining the tutor struct
struct tutorClass {
	int tutorId;			// The id of the tutor
};


// Defining the Coordinator struct
struct coordinatorClass {
	int coordinatorId;		// The id of the coordinator
	int counterStart;		// index used to know the last location of the chairs that the coordinator added in the queue
	int counterEnd;			// index of the last location of the chair that the student occupied
	int* studentId;			// array of int maintained to know the id's of the students who have occupied chairs recently
};

// Defining the linkedList struct
// This linked list will be used for the priority queue
struct linkedList {
	struct studentClass studentObject;	// Stores the student object
	struct linkedList *next;			// Stores the location of next node
};

// Initiating the head and tail nodes of the priority queue
struct linkedList *head = NULL;
struct linkedList *tail = NULL;


// *************** Priority Queue Functions *******************

// To check if the priority queue is empty or not
bool isEmpty(){
	if(head == NULL && tail == NULL) return true;
	return false;
}

// Add a student node into the priority queue
// This function will rearrange the order of the linked list based on the priority of the new student
// This function makes sure that the student nodes with high priority stay ahead of student nodes with low priority maintaining the property of the priority queue
// P0 - Highest Priority. Student who hasn't yet received any help will be prioritized first.
void priorityPush(struct studentClass studentObject){

	// Create the node to be pushed into the Priority Queue (PQ)
	struct linkedList *temp = (struct linkedList*) malloc(sizeof(struct linkedList));
	temp->studentObject = studentObject;
	temp->next = NULL;
	
	// If PQ is empty initiate with the available student node
	if(isEmpty()) {
		head = temp;
		tail = temp;
	}

	else {

		// Have to insert the student node at the correct place to maintain the priority queue

		// Getting the priority of the node to be inserted
		int priority;
		priority = temp->studentObject.numOfHelpsTaken;

		// if the tail contains more prioritized student node then insert at the last
		// We are able to do this because we maintain the PQ for every node we insert
		if(tail->studentObject.numOfHelpsTaken <= priority){
			// inserting the node at the end of PQ
			tail->next = temp;
			tail = temp;
			return;
		} 

		// if the head contains less prioritized student node then insert at the first
		else if(head->studentObject.numOfHelpsTaken > priority){
			temp->next = head;
			head = temp;
			return;
		} 

		// Insert the student node making sure the property of PQ remains intact.
		else{
			struct linkedList* finder;
			finder = head;
			while(finder->next != NULL){

				// If we were able to find the suitable location to be inserted, insert there.
				if(finder->next->studentObject.numOfHelpsTaken > priority){
					temp->next = finder->next;
					finder->next = temp;
					break;
				}

				// If not the correct place to insert, move forward
				finder = finder->next;
			}

			return;
		}


	}
	return;
}

// Since we have maintained the property of the PQ while inserting every node,
// We can just take the first node of the PQ which has the highest priority
struct studentClass pop(){

	// popping the head of the PQ
	struct studentClass output;
	output = head->studentObject;
	if(head == tail){
		head = NULL;
		tail = NULL;
	}else{
		head = head->next;
	}
	return output;

}


// *************** Object declarations *******************
struct studentClass* studentObject; 		
struct tutorClass*   tutorObject;
struct coordinatorClass* coordinatorObject;



// *************** Variable declarations *******************

// console inputs
int numOfStudents;
int numOfChairs;
int numOfTutors;
int numOfCoordinators;

// student print
int numOfAvailableChairs;
int numOfHelpsRequired;

// coordinator print
int numOfTotalRequests = 0;
int numOfStudentsWaiting = 0;

// Tutor print
int numOfStudentsReceivingHelpNow = 0;
int numOfTutoredSessions = 0;

// variable for code help
int numOfStudentsCompletedProgramming = 0;


// *************** Locks and Semaphore declarations *******************

// Lock declarations
pthread_mutex_t availableChairs;				// Used to lock the number of chairs available
pthread_mutex_t TutoredSessions;				// Locking the tutored sessions as it is a shared variable
pthread_mutex_t studentsCompletedProgramming;	// Locking this shared variable
pthread_mutex_t queue;							// Locking operations to the priority queue
pthread_mutex_t coordinatorDetails;				// Locking the coordinator object
pthread_mutex_t studentsReceivingHelpNow;		// Locking this shared variable

// Semaphore declarations
sem_t studentCoordinator;						// Used to wake up the coordinator when a student arrives
sem_t coordinatorTutor;							// Used to wake up a tutor when a student is added into the priority queue by the coordinator


// *************** Student, Tutor, and Coordinator thread Functions *******************


// student thread
static void *
studentThread(void* studentNode){

	// Getting the student object to whom this thread belongs to
	// We will mostly use the student ID information from the below object
	struct studentClass student = *(struct studentClass*)  studentNode;
	
	// studentObject is the global variable and hence any dynamic data related to student will be obtained from it

	// Running the program till the student gets all the tutoring sessions
	while(studentObject[student.studentId-1].numOfHelpsTaken < numOfHelpsRequired){

		// Student working on the program for a random amount of time upto 2ms
		usleep(rand() %2000);

		// Locking the available chairs to check if there are any chairs available
		pthread_mutex_lock(&availableChairs);

		// if chairs available move ahead
		if(numOfAvailableChairs > 0){

			// Reduce the number of chairs available
			numOfAvailableChairs -= 1;
			printf("S: Student %d takes a seat. Empty Chairs = %d\n", student.studentId, numOfAvailableChairs);

			// Update the coordinator object with student information
			pthread_mutex_lock(&coordinatorDetails);

			// Using the cyclic array approach and maintaining two pointers to get info on arrival of students
			// Adding the student if info, which will be retrived by the coordinator in order.
			coordinatorObject[0].studentId[(coordinatorObject[0].counterEnd)%numOfStudents] = student.studentId;
			coordinatorObject[0].counterEnd += 1;

			// Update complete of the coordinator object
			pthread_mutex_unlock(&coordinatorDetails);

			// Releasing the lock on the num of available chairs
			pthread_mutex_unlock(&availableChairs);
			
			// Waking up the coordinator to push the student into the queue
			sem_post(&studentCoordinator);

			// Spin waiting the student thread, till a tutor is assigned
			while(studentObject[student.studentId-1].tutoringComplete == 0);

			// Unsetting the variable for future use case
			studentObject[student.studentId-1].tutoringComplete = 0;

			// Getting the tutorship for 0.2 ms
			usleep(200);

			// After tutoring is complete, increase the number of helps taken
			studentObject[student.studentId-1].numOfHelpsTaken += 1;	// Lock not required as it is not a shared variable


			printf("S: Student %d received help from tutor %d\n", student.studentId, studentObject[student.studentId-1].tutorIdHelped);
		}

		// if chairs are not available, go back to programming 
		else{
			pthread_mutex_unlock(&availableChairs); // Unlocking if chairs are not available

			printf("S: Student %d found no empty chair. Will try again later.\n", student.studentId);
		}
		
	}

	// Once all the students complete their tutor sessions mark the count of those students to know when to exit from the function
	pthread_mutex_lock(&studentsCompletedProgramming);
	numOfStudentsCompletedProgramming += 1;
	if(numOfStudentsCompletedProgramming == numOfStudents) exit(0); // Exit if all the students completed the required number fo tutor sessions
	pthread_mutex_unlock(&studentsCompletedProgramming);

	pthread_exit(NULL);
}

// Tutor thread
static void *
tutorThread(void* tutorNode){

	// Getting the tutor object to whom this thread belongs to
	// We will mostly use the tutor ID information from the below object
	struct tutorClass tutor = *(struct tutorClass*) tutorNode;
	struct studentClass student;
	
	// Making sure not all students have not completed their tutoring
	while(numOfStudentsCompletedProgramming < numOfStudents){
		
		// Waiting for the coordinator to add student node to the queue
		sem_wait(&coordinatorTutor);

		// Locking the priority queue while getting the most prioritized student 
		pthread_mutex_lock(&queue);

		// Getting the student who needs tutoring next
		student = pop();

		// Decreasing the number of students waiting 
		numOfStudentsWaiting -= 1;
		pthread_mutex_unlock(&queue); // Unlocking the priority queue

		// Updating the student with the tutor information
		studentObject[(student.studentId)-1].tutorIdHelped = tutor.tutorId;

		// letting the student know of the start of the tutoring
		studentObject[student.studentId - 1].tutoringComplete = 1;

		// Increasing the available charis as soon a student goes into the tutoring area
		pthread_mutex_lock(&availableChairs);
		numOfAvailableChairs += 1;
		pthread_mutex_unlock(&availableChairs);

		// Increasing the number of students receiving help
		pthread_mutex_lock(&studentsReceivingHelpNow);
		numOfStudentsReceivingHelpNow += 1;
		pthread_mutex_unlock(&studentsReceivingHelpNow);

		// getting the tutoring 
		usleep(200);

		// Increasing the tutored sessions after tutoring is complete
		pthread_mutex_lock(&TutoredSessions);
		numOfTutoredSessions += 1;
		pthread_mutex_unlock(&TutoredSessions);

		// Decreasing the number of students receiving help at the moment
		pthread_mutex_lock(&studentsReceivingHelpNow);
		numOfStudentsReceivingHelpNow -= 1;
		pthread_mutex_unlock(&studentsReceivingHelpNow);

		printf("T: Student %d tutored by tutor %d. Students tutored now = %d. Total sessions tutored = %d\n", student.studentId, tutor.tutorId, numOfStudentsReceivingHelpNow,numOfTutoredSessions);
	}
	// printf("Tutor thread %d got terminated\n", tutor.tutorId);
	pthread_exit(NULL);
}


// Coordinator thread
static void *
coordinatorThread(void* coordinatorNode){

	// Getting the coordinator object to whom this thread belongs to
	struct coordinatorClass coordinator = *(struct coordinatorClass*) coordinatorNode;
	int i;

	// emptying all the available chairs
	for(i = 0; i < numOfStudents; i++){
		coordinatorObject[0].studentId[i] = 0;
	}

	// Making sure not all students have not completed their tutoring
	while(numOfStudentsCompletedProgramming < numOfStudents){
		struct studentClass student;

		// Waiting for the student to occupy the chair
		sem_wait(&studentCoordinator);

		// locking the coordinator details till all students who occupied the chairs are pushed into the queue
		pthread_mutex_lock(&coordinatorDetails);

		coordinator = coordinatorObject[0];

		// looping through the cyclic array using the two pointers to know who all have arrived new and pushing them into queue one by one
		for(i = coordinator.counterStart; i < coordinator.counterEnd; i++){

			// Locking the priority queue while pushing the student node into it
			pthread_mutex_lock(&queue);

			// Getting the object of the student to be pushed into the priority queue
			student = studentObject[coordinator.studentId[i%numOfStudents] - 1];

			priorityPush(student);

			// Incrementing required variables as necessary
			numOfStudentsWaiting += 1;
			numOfTotalRequests += 1;
			printf("C: Student %d with priority P%d added to the queue. Waiting students now = %d. Total requests = %d\n", student.studentId, student.numOfHelpsTaken, numOfStudentsWaiting, numOfTotalRequests);
			
			// Unlocking the queue for access to others
			pthread_mutex_unlock(&queue);
		}

		// After all new students are accounted for updating the pointer values for next visit
		coordinatorObject[0].counterEnd = coordinatorObject[0].counterEnd % numOfStudents;
		coordinatorObject[0].counterStart = coordinatorObject[0].counterEnd;

		// Unlocking the access to coordinator object
		pthread_mutex_unlock(&coordinatorDetails);

		// Notifying the next available tutor 
		sem_post(&coordinatorTutor);
	}

	pthread_exit(NULL);
}


// *************** Main method *******************

// The driver function
int main(int argc, char* argv[]){

	// Making sure we receive the correct number of parameters
	if(argc != 5){
		printf("Please pass 4 parameters. Try again.\n");
		exit(1);
	}

	// Updating the variables based on console parameters
	numOfStudents = atoi(argv[1]);
	numOfTutors   = atoi(argv[2]);
	numOfChairs   = atoi(argv[3]);
	numOfHelpsRequired = atoi(argv[4]);

	// Updating additional variables used in the code
	numOfAvailableChairs = numOfChairs;
	numOfCoordinators = 1;

	
	// Declaring the types of threads required
	pthread_t *studentPthread;
	pthread_t *tutorPthread;
	pthread_t *coordinatorPthread;

	// Allocating the space as required
	studentPthread = malloc(sizeof(pthread_t) * numOfStudents);
	tutorPthread = malloc(sizeof(pthread_t) * numOfTutors);
	coordinatorPthread = malloc(sizeof(pthread_t) * numOfCoordinators);
	

	// Initiating the required locks
	pthread_mutex_init(&availableChairs,NULL);
	pthread_mutex_init(&TutoredSessions,NULL);
	pthread_mutex_init(&studentsCompletedProgramming, NULL);
	pthread_mutex_init(&queue, NULL);
	pthread_mutex_init(&coordinatorDetails, NULL);
	pthread_mutex_init(&studentsReceivingHelpNow, NULL);

	// Initiating the required semaphores
	sem_init(&studentCoordinator, 0, 0);
	sem_init(&coordinatorTutor, 0, 0);

	// Allocating required space for the student, tutor, and the coordinatore objects
	studentObject = malloc(sizeof(struct studentClass) * numOfStudents);
	tutorObject = malloc(sizeof(struct tutorClass) * numOfTutors);
	coordinatorObject = malloc(sizeof(struct coordinatorClass) * numOfCoordinators);

	int id;
	void* res;

	// Initiating the coordinator thread with required values
	for(id = 0; id < numOfCoordinators; id++){
		coordinatorObject[id].coordinatorId = id+1;
		coordinatorObject[id].counterStart = 0;
		coordinatorObject[id].counterEnd = 0;

		// declaring the size of the chairArray occupied by the students in cyclic manner
		coordinatorObject[id].studentId = malloc(sizeof(int) * numOfStudents);
		assert(pthread_create(&coordinatorPthread[id], NULL, coordinatorThread,(void*) &coordinatorObject[id]) == 0);
	}

	// Initiating the tutor threads
	for(id = 0; id < numOfTutors; id++){
		tutorObject[id].tutorId = id+1;
		assert(pthread_create(&tutorPthread[id], NULL, tutorThread,(void*) &tutorObject[id]) == 0);
	}

	// Initiating the student threads with required values
	for(id = 0; id < numOfStudents; id++){
		studentObject[id].studentId = id+1;
		studentObject[id].numOfHelpsTaken = 0;
		studentObject[id].tutoringComplete = 0;
		assert(pthread_create(&studentPthread[id], NULL, studentThread,(void*) &studentObject[id]) == 0);
	}

	// Waiting for all of student threads to join
	for(id = 0; id < numOfStudents; id++){
		assert(pthread_join(studentPthread[id], &res) == 0);
	}

	// Waiting for all of tutor threads to join
	for(id = 0; id < numOfTutors; id++){
		assert(pthread_join(tutorPthread[id], &res) == 0);
	}

	// Waiting for the coordinator thread to join
	for(id = 0; id < numOfCoordinators; id++){
		assert(pthread_join(coordinatorPthread[id], &res) == 0);
	}

	return 0;
}
