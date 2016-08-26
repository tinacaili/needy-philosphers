/* Authors: Tina Li (ttli@wustl.edu) and Megan Bacani (meganbacani@wustl.edu)
*/

#include <mutex>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

enum states { THINKING, EATING, THIRSTY, DRINKING };

int n;
vector<int> activity;
mutex access_activity;
mutex forks[15];

mutex while_mtx;
int ** matrix;
int queue_num = 0;
int * queue;
mutex phil[15];
int * resources;

/* Dining Philosophers */
void dine_think(){
	int randTime;
	randTime = rand() % 10 + 1;
	access_activity.lock();
	access_activity.unlock();
	sleep(randTime);
}

void dine(){
	int randTime;
	randTime = rand() % 3 + 1;
	access_activity.lock();
	access_activity.unlock();
	sleep(randTime);
}

void dine_print(){
	for (int j = 0; j < activity.size(); j++){
		if (activity[j] == THINKING) cout << " ";
		else cout << "*";
	}
	cout << endl;
}

void * dining_philosopher(void * i){
	int j = *((int *)(i));
	int count = 0;

	while (count < 5){
		dine_think();

		/* One philospher picks up the opposite utensil so there will never be a deadlock */
		if (j == 0){
			forks[(j + 1) % n].lock();
			forks[j].lock();
		}
		else {
			forks[j].lock();
			forks[(j + 1) % n].lock();
		}

		access_activity.lock();
		activity[j] = EATING;
		dine_print();
		access_activity.unlock();

		dine();

		/* Unlock the utensil */
		if (j != 0){
			forks[(j + 1) % n].unlock();
			forks[j].unlock();
		}
		else {
			forks[j].unlock();
			forks[(j + 1) % n].unlock();
		}

		access_activity.lock();
		activity[j] = THINKING;
		dine_print();
		access_activity.unlock();
		count++;
	}

	return nullptr;
}

/* Drinking Philosphers */
void drink_think(){
	int randTime;
	randTime = rand() % 9 + 2;
	access_activity.lock();
	access_activity.unlock();
	sleep(randTime);
}

void drink(){
	int randTime;
	randTime = rand() % 4 + 2;
	access_activity.lock();
	access_activity.unlock();
	sleep(randTime);
	access_activity.lock();
	access_activity.unlock();
}

/* Set up drinks needed for each philo */
int * drink_thirsty(int i, int & r){
	vector<int> bttls;
	for (int j = 0; j < n; j++){
		if (matrix[i][j] == 1){
			bttls.push_back(j);
		}
	}

	int * rand_bttls = new int[bttls.size()];
	for (int j = 0; j < bttls.size(); j++){
		access_activity.lock();
		access_activity.unlock();
		int rnd = rand() % bttls.size();
		while (bttls[rnd] == -1){
			access_activity.lock();
			access_activity.unlock();
			rnd = (rnd + 1) % bttls.size();
		}
		rand_bttls[j] = bttls[rnd];
		bttls[rnd] = -1;
	}

	access_activity.lock();
	access_activity.unlock();
	r = rand() % bttls.size() + 1;

	int * final_bttl = new int[r];
	for (int j = 0; j < r; j++){
		access_activity.lock();
		access_activity.unlock();
		int rand_num = rand() % bttls.size();
		if (rand_bttls[rand_num] != -1){
			final_bttl[j] = rand_bttls[rand_num];
			rand_bttls[rand_num] = -1;
		}
		else j--;
	}

	sort(final_bttl, final_bttl + r);

	delete [] rand_bttls;

	return final_bttl;
}

/* Thirsty philos are put in a queue to prevent dehydration. Drinks other thristy philos hold will 
 * be given to the longest thirsty philo if it can have all its drinks */
void drink_test(int i, int * drink_list, int list_size){
	access_activity.lock();

	bool unlockAll = true;
	for (int j = 0; j < list_size; j++){
		int drink_with = drink_list[j];
		if (activity[drink_with] == DRINKING || (activity[drink_with] == THIRSTY && queue[drink_with] < queue[i])) {
			unlockAll = false;
		}
	}
	if (unlockAll){
		for (int j = 0; j < list_size; j++){
			int drink_with = drink_list[j];
			phil[drink_with].unlock();
			resources[drink_with] = 1;

		}
	}
	access_activity.unlock();

}

/* Check if philospher requesting drink can drink based on available resources
 * and ability to create deadlock */
int can_drink(int i, int * drink_list, int list_size){
	access_activity.lock();

	bool rsrc_avail = true;

	for (int j = 0; j < list_size; j++){
		if (resources[drink_list[j]] == 0) rsrc_avail = false;
	}

	if (resources[i] == 0) rsrc_avail = false;

	access_activity.unlock();

	if (!rsrc_avail) return 0;
	else return 1;
}

/* Locks the drinks the philo needs */
void lock_drinks(int i, int * drink_list, int list_size){
	access_activity.lock();
	activity[i] = THIRSTY;
	int sig_queue = 0;
	queue[i] = queue_num++;
	
	access_activity.unlock();

	while_mtx.lock();
	/* wait for the requested drinks */
	while(!can_drink(i, drink_list, list_size)){
		drink_test(i, drink_list, list_size);
		/* allows other threads a chance to see if their philosphers can drink */
		while_mtx.unlock();
		/* keep while mutex locked so no other threads can get in while
		* a philo a trying to determin if they can drink and if they can,
		* appropriate resources can be updated before the next philosphers
		* test */
		while_mtx.lock();
	}

	/* philospher has acquired all the bottles he needs! */
	access_activity.lock();

	phil[i].lock();
	resources[i] = 0;
	
	/* update the resource list and lock the drinks the philo needs*/
	for (int j = 0; j < list_size; j++){
		phil[drink_list[j]].lock();
		resources[drink_list[j]] = 0;
	}
	while_mtx.unlock();

	activity[i] = DRINKING;

	cout << "Philospher " << i << ": drinking from bottles ";
	for (int j = 0; j < list_size; j++){
		if (i > drink_list[j]) cout << "(" << drink_list[j] << ", " << i << ")";
		else cout << "(" << i << ", " << drink_list[j] << ")";
		if (j < list_size - 1) cout << ", ";
		else cout << ".";
	}	
	cout << endl;

	access_activity.unlock();
}

/* Unlock drinks after philo is done and update resource list */
void unlock_drinks(int i, int * drink_list, int list_size){
	access_activity.lock();

	phil[i].unlock();
	resources[i] = 1;

	for (int j = 0; j < list_size; j++){
		phil[drink_list[j]].unlock();
		resources[drink_list[j]] = 1;
	}

	activity[i] = THINKING;

	cout << "Philospher " << i << ": putting down bottles ";
	for (int j = 0; j < list_size; j++){
		if (i > drink_list[j]) cout << "(" << drink_list[j] << ", " << i << ")";
		else cout << "(" << i << ", " << drink_list[j] << ")";
		if (j < list_size - 1) cout << ", ";
		else cout << ".";
	}
	cout << endl;
	access_activity.unlock();
}

void * drinking_philosopher(void * i){

	int j = *((int *)(i));
	int count = 0;

	while (count < 5){
		drink_think();

		int list_size;
		int * drink_list = drink_thirsty(j, list_size);
		
		lock_drinks(j, drink_list, list_size);
		drink();
		unlock_drinks(j, drink_list, list_size);

		count++;
	}

	return nullptr;
}


int main(int argc, const char * argv[])
{
	string dd = "";
	while (dd != "dine" && dd != "drink"){
		cout << "'dine' or 'drink'" << endl;
		cin >> dd;
	}

	srand(time(NULL));
	n = 0;

	if (dd == "dine"){
		string s;
		while (!(1 < n && n <= 15)) {
			cout << "How many philosophers (2-15)? Put in a number" << endl;
			cin >> s;
			n = stoi(s);
		}
		cout << endl;

		cout << "Eating Activity" << endl;
		for (int i = 0; i < n; i++){
			activity.push_back(THINKING);
			if (i >= 10) cout << "1";
			else cout << " ";
		}
		cout << endl;

		for (int i = 0; i < n; i++){
			cout << i % 10;
		}
		cout << endl;

		pthread_t * tharr = new pthread_t[n];
		vector<int> counters;
		for (int i = 0; i < n; i++){
			counters.push_back(i);
		}
		for (int i = 0; i < n; i++){
			pthread_create(&tharr[i], nullptr, dining_philosopher, &counters[i]);
		}
		for (int i = 0; i < n; i++){
			pthread_join(tharr[i], nullptr);
		}

		delete[] tharr;
	}
	else{
		string filename;
		cout << "Input file:" << endl;
		cin >> filename;
		fstream ifs(filename);
		if (ifs){
			string line;
			vector<string> vec_lines;
			while (getline(ifs, line)){
				vec_lines.push_back(line);
			}

			n = vec_lines.size();
			matrix = new int *[n];
			for (int i = 0; i < n; i++){
				matrix[i] = new int[n];
			}

			for (int i = 0; i < n; i++){
				stringstream ss(vec_lines[i]);
				string s_str;
				int j = i;
				for (int k = 0; k < j; k++){
					matrix[i][k] = matrix[k][i];
				}
				while (ss >> s_str){
					int n = stoi(s_str);
					matrix[i][j] = n;
					j++;
				}
			}

			resources = new int[n];
			queue = new int[n];
			vector<int> counters;
			cout << endl;
			cout << "Drinking Activity" << endl;
			for (int i = 0; i < n; i++){
				activity.push_back(THINKING);
				resources[i] = 1;
				queue[i] = 0;
				counters.push_back(i);
			}

			pthread_t * tharr = new pthread_t[n];
			for (int i = 0; i < n; i++){
				pthread_create(&tharr[i], nullptr, drinking_philosopher, &counters[i]);
			}
			for (int i = 0; i < n; i++){
				pthread_join(tharr[i], nullptr);
			}

			delete[] matrix;
			delete[] resources;
			delete[] queue;
			delete[] tharr;
		}
		else{
			cout << "File not found" << endl;
		}
	}
	return 0;
}

