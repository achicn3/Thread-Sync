#include <iostream>
#include <pthread.h>
#include <queue>
#include <string>
#include <sstream>
#include <unistd.h>
#include <chrono>
#include <cstring>
//One second = 10^6 microsecond
#define seconds 1000000
#define helpTime 2*seconds
#define chairs 3
#define person_TA "TA"
using namespace std;

//Fifo Mutex lock
pthread_mutex_t fifoMutex;
//When serve 60 students, exit program.
int serve_students = 0;


int ta_napping = 1;

struct Students {
    Students(const string &number, int waitingTime, int intervalTime) : number(number),
                                                                        waitingTime(waitingTime),
                                                                        intervalTime(intervalTime) {}

    //# of person_student
    string number = "Student1";
    //Check if student is first time ask ta.
    int isFirstTimeAsk = 1;
    //Check if student should wait before ask ta.
    int shouldWait = 1;
    //If student is first time ask ta, this is wait time.
    int waitingTime = 0;
    //The waiting time before to ask ta.
    int intervalTime = 0;
};

//Serve queue.
queue<Students> fifo;

//Print message.
void print_msg(const string &person, const string &msg) {
    //Get current time.
    time_t currentTime;
    struct tm *localTime;
    time(&currentTime);
    localTime = localtime(&currentTime);
    char times[100] = {'\0'};
    int hour = localTime->tm_hour;
    int min = localTime->tm_min;
    int sec = localTime->tm_sec;
    //Pad leading zero of time.
    sprintf(times, "%02d:%02d:%02d", hour, min, sec);
    cout << times << " " << person << ": " << msg << "\n";
}

void *enqueue(void *arg) {
    Students students = *(Students *) arg;
    while (1) {
        pthread_mutex_lock(&fifoMutex);
        //If student is first time ask ta, and student should wait
        //Then wait waitingTime seconds.
        if (students.isFirstTimeAsk && students.shouldWait) {
            students.isFirstTimeAsk = 0;
            students.shouldWait = 0;
            pthread_mutex_unlock(&fifoMutex);
            usleep(students.waitingTime * seconds);
            continue;
        } else if (students.shouldWait) {
            //If student should wait, then wait intervalTime seconds.
            students.shouldWait = 0;
            pthread_mutex_unlock(&fifoMutex);
            usleep(students.intervalTime * seconds);
            continue;
        }
        //If chairs is not full.
        if (fifo.size() < chairs) {
            //Prevent same students show in queue multiple time.
            int isInside = 0;
            //Iterate queue to check if the student have in queue.
            int fifoSize = fifo.size();
            students.shouldWait = 1;
            for (int i = 0; i < fifoSize; ++i) {
                Students stu = fifo.front();
                fifo.pop();
                if (stu.number == students.number) {
                    isInside = 1;
                }
                fifo.push(stu);
            }
            //OK, student is really not in queue.
            //Push student into queue.
            if (!isInside) {
                fifo.push(students);
                ta_napping = 0;
            }
            //Unlock mutex.
            pthread_mutex_unlock(&fifoMutex);
        } else {
            //Chair is full, unlock mutex first.
            pthread_mutex_unlock(&fifoMutex);
            while (1) {
                //Student will come back after 3 seconds.
                usleep(3 * seconds);
                //Lock mutex first.
                pthread_mutex_lock(&fifoMutex);
                //Check if chair is not full
                if (fifo.size() < 3) {
                    //Chair not full, then sit down
                    fifo.push(students);
                    //Wake up ta.
                    ta_napping = 0;
                    //Unlock mutex
                    pthread_mutex_unlock(&fifoMutex);
                    break;
                }
                //If chairs is full, then just unlock, student will come back again .
                pthread_mutex_unlock(&fifoMutex);
            }
        }
    }
}

//For ta to serve student.
void *serve(void *arg) {
    while (1) {
        //Lock mutex first.
        pthread_mutex_lock(&fifoMutex);
        //Check if ta serve 60 students.
        if (serve_students == 60) {
            print_msg(person_TA, "Office is closed.");
            exit(1);
        }
        //Ok. fifo is empty.
        if (fifo.empty()) {
            //Check should print ta napping.
            //Prevent print multiple time.
            if (ta_napping) {
                print_msg(person_TA, "TA napping.");
                ta_napping = 0;
            }
        } else {
            //Fifo not empty.
            //Then serve serve_student.
            Students serve_student = fifo.front();
            fifo.pop();
            //serve serve_student
            print_msg(serve_student.number, "Asking TA");
            serve_students++;
            int fifoSize = fifo.size();
            //Print waiting serve_student.
            for (int i = 0; i < fifoSize; ++i) {
                Students wait_student = fifo.front();
                fifo.pop();
                print_msg(wait_student.number, "Sitting #" + to_string(i + 1));
                fifo.push(wait_student);
            }
            print_msg(serve_student.number, "Leaving to write programs.");
            //OK, fifo is empty now, print ta napping next time, if fifo is still empty.
            if (fifo.empty()) {
                ta_napping = 1;
            }
            //Serve student need 2 seconds.
            usleep(2 * seconds);
        }
        //Unlock mutex.
        pthread_mutex_unlock(&fifoMutex);
    }
}

int validNumber(char str[]){
    for(int i=0;i<strlen(str);++i){
        if(!(str[i]>='0' && str[i]<='9') )return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cout << "Error! usage:" << argv[0] << " [num_of_people] [seed]\n";
        exit(1);
    }
    /**
     * Get number of students
     * */
    int students;
    stringstream ss(argv[1]);
    //Store number of students in students var.
    ss >> students;
    //Check if number of students is valid.
    if (students < 10 || students > 20 || !validNumber(argv[1])) {
        cout << "Error! Number of students must in range 10~20!\n";
        exit(1);
    }
    int seed;
    //Store seed.
    stringstream ss2(argv[2]);
    ss2 >> seed;
    //Check if seed is valid.
    if (seed < 0 || seed > 100 || !validNumber(argv[2])) {
        cout << "Error! Seed must in range 0~100!\n";
        exit(1);
    }
    srand(seed);
    pthread_t pthread[students];
    pthread_t ta;
    pthread_mutex_init(&fifoMutex, nullptr);
    //Create TA thread.
    pthread_create(&ta, nullptr, serve, nullptr);
    for (int i = 0; i < students; ++i) {
        //Padding zero before student ID if student ID lower than 10.
        string studentID = (i + 1) >= 10 ? to_string(i + 1) : "0" + to_string(i + 1);
        //Create N (number of students) thread.
        Students st = Students{"Student" + studentID, rand() % 31, rand() % 20 + 11};
        pthread_create(&pthread[i], nullptr, enqueue, (void *) &st);
    }
    pthread_join(ta, 0);
    for (int i = 0; i < students; ++i) {
        pthread_join(pthread[i], 0);
    }
    return 0;
}
