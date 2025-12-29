#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <execution>
#include <algorithm>
#include <mutex>
#include <ApplicationServices/ApplicationServices.h>
#include "state.h"

#define NUM_THREADS 2

using namespace std;

const int N = 100;
const int v_len = 17;
const double Z = 4;
const double ro = 0.1;

default_random_engine generator;

void simulate(){
    double cumScore = 0;
    int numGames = 0;
    srand(time(NULL));
    while(true){
        numGames++;
        State * state = new State();
        State * initState = state;
        State * sampled = state->sampleState();
        state = sampled;

        while(true){
            state->print();
            //getchar();
            if(state == NULL){
                cerr << "NULL STATE wtf" << endl;
            }
            if (state->isTerminal()){
                break;
            }
            
            // state = domove(state);

            State * sampled = state->sampleState();

            state = sampled;
        }

        cumScore += state->score;
        cout << "score = " << state->score << endl;
        cout << "average score: " << cumScore / numGames << endl;
        cout << "num games: " << numGames << endl;
        delete initState;
        delete state;
    }
}

void generateVectors(vector<vector<double> > & V, vector<double> & mus, vector<double> & sigmas, int len) {

	vector<normal_distribution<double> > dists;
	for (int i = 0; i < len; i++) {
		dists.push_back(normal_distribution<double>(mus[i], sigmas[i]));
	}
	for (int i = 0; i < N; i++) {
		vector<double> v;
		for (int j = 0; j < len; j++) {
			double r = dists[j](generator);

			v.push_back(r);
		}
		V.push_back(v);
	}
}

double evalState(State * state, vector<double> & w){
	if (state->curCard != -1 && state->nextCard == -1){ // put curcard back in deck
		state->left[state->curCard]++;
		state->cardsLeft++;
	}
    int v[v_len] = {0};
    v[0] = state->streak == 0;
	v[1] = state->streak == 1;
	v[2] = state->streak == 2;
	v[3] = state->streak >= 3;
    v[4] = state->score;
	int needed[11] = {0};
	int needed11[11] = {0};
	double prob = 0;
	int numSpaces = 0;
	int numBelow11 = 0;
	for(int i = 0; i <4; i++){
		if (21 - state->totals[i] < 11){
			prob += (state->left[21 - state->totals[i]]) / (double)state->cardsLeft;
			needed[21 - state->totals[i]]++;
		} else if (state->soft[i]){
			prob += (state->left[11 - state->totals[i]]) / (double)state->cardsLeft;
			needed[11 - state->totals[i]]++;
		}
		if (state->totals[i] == 0){
			numSpaces++;
		} else if(state->totals[i] < 11){
			numBelow11++;
			needed11[11 - state->totals[i]]++;
		}
	}
	v[5] = prob;

	double uniqueProb = 0;
	double unique11Prob = 0;
	for(int i = 0; i <= 10; i++){
		if (needed[i]){
			uniqueProb += (state->left[i]) / (double)state->cardsLeft;
		}
		if (needed11[i]){
			unique11Prob += (state->left[i]) / (double)state->cardsLeft;
		}
	}
	v[6] = uniqueProb;
	v[7] = unique11Prob;
    
    v[8] = state->hold == -1;
    v[9] = state->holdsLeft;
	v[10] = state->hasBusted;
	v[11] = numSpaces;
	v[12] = numBelow11;
	v[13] = 0;
	// hold can make 21
	if (state->hold != -1){
		for(int i = 0; i < 4; i++){
			if (state->totals[i] + state->hold == 21 || 
				(state->totals[i] + state->hold == 11 && (state->soft[i] || state->hold == 1)) ||
				(state->totals[i] + state->hold < 21 && state->numCards[i] == 4)){
				v[12]++;
			}
		}
	}

	v[14] = 0;
	// curCard can make 21
	if (state->curCard != -1 && state->nextCard != -1){
		for(int i = 0; i < 4; i++){
			if (state->totals[i] + state->curCard == 21 || 
				(state->totals[i] + state->curCard == 11 && (state->soft[i] || state->curCard == 1)) ||
				(state->totals[i] + state->curCard < 21 && state->numCards[i] == 4)){
				v[13]++;
			}
		}
	}

	// nextCard can make 21
	v[15] = 0;
	if (state->nextCard != -1){
		for(int i = 0; i < 4; i++){
			if (state->totals[i] + state->nextCard == 21 || 
				(state->totals[i] + state->nextCard == 11 && (state->soft[i] || state->nextCard == 1)) || 
				(state->totals[i] + state->nextCard < 21 && state->numCards[i] == 4)){
				v[14]++;
			}
		}
	}
	v[16] = v[12] > 0 && v[13] > 0;
    
	double score = 0;
	for (int i = 0; i < v_len; i++) {
		score += v[i] * w[i];
	}
    return score;
}

State * getBestMove(State * state, vector<double> & w){
	vector<State *> children = state->getAvailableStates();

	if (children.size() == 0){
		cerr << "WTF" << endl;
		state->print();
		getchar();
	}
	State * bestChild;
	double bestScore = -999999999;
	for(int i = 0; i < children.size(); i++){
		State * sampled = children[i]->sampleState();
		double score = evalState(sampled, w);
		if(score > bestScore){
			bestScore = score;
			bestChild = children[i];
		}
		delete sampled;
	}

	// delete all children except bestChild
	for(int i = 0; i < children.size(); i++){
		if(children[i] != bestChild){
			delete children[i];
		}
	}

	return bestChild;
}

double evaluate(vector<double> & w, bool print = false){
    State * state = new State();
    State * sampled = state->sampleState();
	delete state;
	state = sampled;
    while(true){
        if(state == NULL){
            cerr << "NULL STATE wtf" << endl;
        }
        if (state->isTerminal()){
            break;
        }
        State * bestState = getBestMove(state, w);
        State * sampled = bestState->sampleState();

        delete state;
        delete bestState;
        state = sampled;
    }
    double score = state->score;

	if (state != NULL){
    	delete state;
		state = NULL;
	}

    return score;
}

void getScores(int begin, int end, vector<pair<double, int> > & scores, vector<vector<double> > & V){
	for (int i = begin; i < end; i++){
		double totscore = 0;
		for (int j = 0; j < 100; j++){
			totscore += evaluate(V[i]);
		}
		scores[i] = { totscore / 100, i };
	}
}

void crossentropy(){
	vector<double> mus;
	vector<double> sigmas;
    
	for (int i = 0; i < v_len; i++) {
		mus.push_back(0);
		sigmas.push_back(100);
	}
	vector<vector<double> > bestCandidates;
	double bestScore = 0;
	vector<double> bestV;

    for(int iter = 0; iter < 50; iter++){
		cout << "iteration " << iter << endl;
		vector<vector<double> > V;
		generateVectors(V, mus, sigmas, v_len);

		vector<pair<double, int> > scores;
		scores.resize(N);

		thread t[NUM_THREADS];
		for (int i = 0; i < NUM_THREADS; i++){
			t[i] = thread(getScores, i * N / NUM_THREADS, (i + 1) * N / NUM_THREADS, ref(scores), ref(V));
		}
		for (int i = 0; i < NUM_THREADS; i++){
			t[i].join();
		}
		sort(scores.rbegin(), scores.rend());
		bestCandidates.clear();

        //get top X % of candidates
		for (int i = 0; i < ro * N; i++) {
			bestCandidates.push_back(V[scores[i].second]);
		}
        
        //calculate new mus and sigmas from top candidates
		for (int i = 0; i < v_len; i++) {
			double mu = 0;
			double sigma = 0;
			for (int j = 0; j < bestCandidates.size(); j++) {
				mu += bestCandidates[j][i];
			}
			mu /= bestCandidates.size();

			for (int j = 0; j < bestCandidates.size(); j++) {
				sigma += (bestCandidates[j][i] - mu) * (bestCandidates[j][i] - mu);
			}
			sigma /= bestCandidates.size();
			sigma = sqrt(sigma);

			mus[i] = mu;
			sigmas[i] = sigma + Z;
		}

		for (int i = 0; i < ro * N; i++) {
			cout << scores[i].first << " ";
		}
		cout << endl;
        double avg = 0;
		for (int i = 0; i < 100; i++) {
			avg += evaluate(bestCandidates[0]);
		}
		if (avg / 100 > bestScore) {
			bestScore = avg / 100;
			bestV = bestCandidates[0];
		}
		cout << "avg = " << avg / 100 << endl;
		for (int i = 0; i < v_len; i++){
			cout << bestCandidates[0][i] << ", ";
		}
		cout << endl;
    }
    
	for (int i = 0; i < bestV.size(); i++) {
		cout << bestV[i] << ", ";
	}
    
	double avg = 0;
	for (int i = 0; i < 200; i++) {
		avg += evaluate(bestV);
	}
	avg /= 200;
	cout << endl;
	cout << avg << endl;
}

int main() {
    crossentropy();
	//simulate();
    return 0;
}