#pragma once

#include <vector>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <utility>
#include "overlay.h"

using namespace std;

struct State {
    // When sampling from screen/window captures, `fromPixels()` uses hardcoded
    // screen-space coordinates (x,y). These thread-local parameters allow callers
    // to map those coordinates into the coordinate system of the provided pixel
    // buffer (e.g., a window-only screenshot).
    static inline thread_local double captureOffsetX = 0.0;
    static inline thread_local double captureOffsetY = 0.0;
    static inline thread_local double captureScaleX = 1.0;
    static inline thread_local double captureScaleY = 1.0;

    static inline void setCaptureTransform(double offsetX, double offsetY, double scaleX, double scaleY) {
        captureOffsetX = offsetX;
        captureOffsetY = offsetY;
        captureScaleX = scaleX;
        captureScaleY = scaleY;
    }

    static inline void resetCaptureTransform() {
        setCaptureTransform(0.0, 0.0, 1.0, 1.0);
    }

    int totals[4];
    int numCards[4];
    bool soft[4];
    int left[11];
    int cardsLeft;
    int curCard;
    int streak;
    int score;
    int nextCard;
    int nextNextCard;
    int prevCard;
    bool canUndo;
    bool justUndid;
    bool wasSoft;
    bool wasBusted;
    int prevStreak;
    int prevScore;
    int lastPos;
    int prevTotal;
    int prevNumCards;
    int undoCounter;
    int numUndo;

    bool hasBusted;
    int curMove;

    int curSuit;
    int curRank;

    State(){
        for(int i=0;i<4;i++){
            totals[i] = 0;
            numCards[i] = 0;
            soft[i] = false;
        }
        for(int i=0;i<11;i++){
            left[i] = 4;
        }
        left[10] = 14;
        left[0] = 2;
        cardsLeft = 52;
        curCard = -1;
        streak = 0;
        score = 0;
        nextCard = -1;
        nextNextCard = -1;
        prevCard = -1;
        undoCounter = 0;
        numUndo = 0;
        hasBusted = false;
        curMove = -1;
    }
    long long simpleHash(){
        long long hashcode = 0;
        for(int i = 0; i < 4; i++){
            hashcode = hashcode * 22ll + totals[i];
            hashcode = hashcode * 5ll + min(1,numCards[i]);
            hashcode = hashcode * 2ll + soft[i];
        }

        if (curCard != -1){
            left[curCard]++;
        }
        hashcode = hashcode * 3 + left[0];
        for(int i = 1; i < 10; i++){
            hashcode = hashcode * 5ll + left[i];
        }
        hashcode = hashcode * 15ll + left[10];

        if (curCard != -1){
            left[curCard]--;
        }
        return hashcode;
    }

    State(State * state){
        for(int i=0;i<4;i++){
            totals[i] = state->totals[i];
            numCards[i] = state->numCards[i];
            soft[i] = state->soft[i];
        }
        for(int i=0;i<11;i++){
            left[i] = state->left[i];
        }
        cardsLeft = state->cardsLeft;
        curCard = state->curCard;
        streak = state->streak;
        score = state->score;
        nextCard = state->nextCard;
        nextNextCard = state->nextNextCard;
        prevCard = state->prevCard;
        canUndo = state->canUndo;
        wasSoft = state->wasSoft;
        wasBusted = state->wasBusted;
        prevStreak = state->prevStreak;
        prevScore = state->prevScore;
        lastPos = state->lastPos;
        prevTotal = state->prevTotal;
        prevNumCards = state->prevNumCards;
        undoCounter = state->undoCounter;
        numUndo = state->numUndo;
        hasBusted = state->hasBusted;
        curMove = state->curMove;
    }

    State * sampleState(int fixedCard = -1) {
        State * result = new State(this);
        result->undoCounter--;
        if (fixedCard != -1){
            result->curSuit = fixedCard % 4;
            result->curRank = fixedCard / 4;
        }

        if (justUndid){
            result->justUndid = false;
            result->nextCard = result->curCard;
            result->curCard = result->prevCard;
            if (result->nextCard != -1){
                result->left[result->nextCard]++;
                result->cardsLeft++;
            }

            result->prevCard = -1;
            if (result->totals[result->lastPos] == 0){ // if we undid a bust
                result->score = result->prevScore;
                result->totals[result->lastPos] = result->prevTotal;
                result->soft[result->lastPos] = result->wasSoft;
                result->numCards[result->lastPos] = result->prevNumCards;
            } else {
                result->numCards[result->lastPos]--;
                result->totals[result->lastPos]-=result->curCard;
                result->soft[result->lastPos] = result->wasSoft;
            }
            
            result->canUndo = false;
            result->undoCounter = 2;

            result->streak = result->prevStreak;
            result->score -= 1;
            if(result->score < 0){
                result->score = 0;
            }
            result->numUndo++;
            return result;
        }
        if (nextCard != -1){
            result->curCard = nextCard;
            result->nextCard = result->nextNextCard;
            result->nextNextCard = -1;
            result->left[nextCard]--;
        } else {
            if (result->cardsLeft > 0){
                int card = rand() % result->cardsLeft;
                int i = 0;
                while(card >= result->left[i]){
                    card -= result->left[i];
                    i++;
                }

                if (fixedCard != -1){
                    if (result->curRank == 10 && result->curSuit >= 2){
                        result->curCard = 0;
                    } else if (result->curRank >= 9){
                        result->curCard = 10;
                    } else {
                        result->curCard = result->curRank + 1;
                    }
                } else {
                    result->curCard = i;
                }
                result->left[result->curCard]--;
            } else if (result->curCard >= 0){
                result->curCard = -1;
            }
        }

        if (result->curMove == -1 && result->cardsLeft == 0){
            return result;
        }
        if (result->cardsLeft > 0){
            result->cardsLeft--;
        }

        if (result->curMove == -1){
            return result;
        }

        result->prevScore = result->score;
        result->prevStreak = result->streak;
        result->prevNumCards = result->numCards[curMove];
        result->prevTotal = result->totals[curMove];
        result->wasSoft = result->soft[curMove];
        if (curCard == 0){ // wild card

            result->lastPos = curMove;
            if (result->undoCounter <= 0){
                result->canUndo = true;
            }
            result->prevCard = curCard;

            if (streak >= 5){
                result->score += 125;
            } else if (streak >= 4){
                result->score += 100;
            } else if (streak >= 3){
                result->score += 75;
            } else if (streak >= 2){
                result->score += 50;
            } else if (streak >= 1){
                result->score += 25;
            }
            result->score += 20;
            if (result->numCards[curMove] >= 4){
                result->score += 60;
            }
            if (result->totals[curMove] == 11 || result->totals[curMove] == 1){
                result->score += 40;
            }
            result->totals[curMove] = 0;
            result->numCards[curMove] = 0;
            result->streak++;
            result->streak = min(result->streak, 5);
            result->soft[curMove] = false;
        } else if (curCard > 0){
            result->totals[curMove]+=curCard;
            result->numCards[curMove]++;
            if (result->totals[curMove] <= 11 && curCard == 1){
                result->soft[curMove] = true;
            }
            bool cleared = false;
            if (result->totals[curMove] == 21 || (result->totals[curMove] == 11 && result->soft[curMove])
                || (result->numCards[curMove] >= 5 && result->totals[curMove] <= 21)){

                result->lastPos = curMove;
                if (result->undoCounter <= 0){
                    result->canUndo = true;
                }
                result->prevCard = curCard;

                if (streak >= 5){
                    result->score += 125;
                } else if (streak >= 4){
                    result->score += 100;
                } else if (streak >= 3){
                    result->score += 75;
                } else if (streak >= 2){
                    result->score += 50;
                } else if (streak >= 1){
                    result->score += 25;
                }
                if (result->numCards[curMove] >= 5){
                    result->score += 60;
                }
                if (result->totals[curMove] == 21 || (result->totals[curMove] == 11 && result->soft[curMove])){
                    result->score += 40;
                }
                result->streak++;
                result->streak = min(result->streak, 5);
                cleared = true;
            } else if (result->totals[curMove] > 21){
                result->lastPos = curMove;
                if (result->undoCounter <= 0){
                    result->canUndo = true;
                }
                result->prevCard = curCard;
                result->hasBusted = true;
                cleared = true;
                result->streak = 0;
            } else {
                result->streak = 0;
                if (result->totals[curMove] > 11){
                    result->soft[curMove] = false;
                }
                result->lastPos = curMove;
                if (result->undoCounter <= 0){
                    result->canUndo = true;
                }
                result->prevCard = curCard;
            }
            if (cleared){
                result->totals[curMove] = 0;
                result->numCards[curMove] = 0;
                result->soft[curMove] = false;
            }
        }
        if (result->cardsLeft == 0 && result->curCard == -1){
            if (!result->hasBusted){
                result->score += 10;
                if (result->numCards[0] == 0 && result->numCards[1] == 0 && result->numCards[2] == 0 && result->numCards[3] == 0){
                    result->score += 100;
                }
            }
        }

        result->curMove = -1;
        return result;
    }

    vector<State *> getAvailableStates() { // Returns the legal moves.
        vector<State *> result;

        if (isTerminal()) {
            return result;
        }

        int numSpaces = 0;
        for(int i = 0; i < 4; i++){
            if (totals[i] == 0) numSpaces++;
        }
        int numUndoSlots = 0;
        if (curCard >= 0){
            int undoTotal = totals[lastPos] - prevCard;
            int undoNumCards = numCards[lastPos] - 1;
            for(int i = 0; i < 4; i++){
                if (nextCard != -1 && i == lastPos && undoCounter == 2) continue;
                bool isDuplicate = false;
                for(int j = 0; j < i; j++){
                    if (totals[i] == totals[j] && numCards[i] == numCards[j] && soft[i] == soft[j]){
                        isDuplicate = true;
                        break;
                    }
                }
                if (isDuplicate){
                    continue;
                }
                if (i != lastPos && totals[i] + prevCard <= 21){
                    if(totals[i] == prevTotal && numCards[i] == prevNumCards && soft[i] == wasSoft){
                    } else {
                        numUndoSlots++;
                    }
                }
                if (totals[i] + curCard > 21 && numSpaces > 0){
                    continue;
                }

                State * newState = new State(this);
                newState->curMove = i;
                result.push_back(newState);
            }
        }

        if (canUndo && numUndoSlots > 0){ // undo
            State * newState = new State(this);
            newState->justUndid = true;
            newState->canUndo = false;
            result.push_back(newState);
        }
        return result;
    }

    bool isTerminal() const { // Returns true if the game is over.
        return cardsLeft == 0 && curCard == -1;
    }

    void print() const { // Prints the current state.
        cerr << "totals: ";
        for(int i=0;i<4;i++){
            cerr << totals[i] << " ";
        }
        cerr << endl;
        cerr << "numCards: ";
        for(int i=0;i<4;i++){
            cerr << numCards[i] << " ";
        }
        cerr << endl;
        cerr << "soft: ";
        for(int i=0;i<4;i++){
            cerr << soft[i] << " ";
        }
        cerr << endl;
        cerr << "left: ";
        for(int i=0;i<11;i++){
            cerr << left[i] << " ";
        }
        cerr << endl;
        cerr << "curCard: " << curCard << endl;
        cerr << "cardsLeft: " << cardsLeft << ", ";
        cerr << "score: " << score << ", ";
        cerr << "streak: " << streak << ", ";
        cerr << "justUndid: " << justUndid << ", ";
        cerr << "canUndo: " << canUndo << ", ";
        cerr << "lastPos: " << lastPos << ", ";
        cerr << "nextCard: " << nextCard << ", ";
        cerr << "nextNextCard: " << nextNextCard << ", ";
        cerr << "hasBusted: " << hasBusted << ", ";
        cerr << "curMove: " << curMove << ", ";
        cerr << "prevCard: " << prevCard << ", ";
        cerr << "undoCounter: " << undoCounter << ", ";
        cerr << "numUndo: " << numUndo << ", ";
        cerr << endl;
    }

    void showBestMove(int windowX, int windowY, int windowW, int windowH) const {
        if (justUndid){
            overlay_set_text_position(windowX + windowW / 2, windowY + windowH / 2);
            overlay_set_text_utf8("undo");
        } else {
            double posX = windowX + windowW * curMove / 5 + windowW / 8;
            double posY = windowY + windowH / 2;
            overlay_set_text_position(posX, posY);
            overlay_set_text_utf8(to_string(curCard).c_str());
        }
    }

    long long hash_code() const { // Returns a hash code for the current state.
        long long result = 0;
        for(int i=0;i<4;i++){
            result = result * 23 + totals[i];
        }
        for(int i=0;i<4;i++){
            result = result * 5 + numCards[i];
        }
        for(int i=0;i<11;i++){
            result = result * 5 + left[i];
        }
        result = result * 53 + cardsLeft;
        result = result * 13 + curCard;
        result = result * 7 + streak;
        result = result * 293 + score;
        result = result * 13 + (nextCard+1);
        result = result * 2 + hasBusted;
        result = result * 7 + (curMove+1);

        return result;
    }

    bool makeRandomMove(){
        int move = rand() % 4;
        curMove = move;
        return true;
    }

    bool makeSmartMove(){
        int move = -1;
        int bestScore = -1000;
        // overrides
         //   print();
        
        if (prevStreak >= 1 && streak == 0 && canUndo && lastPos >= 0 && curCard >= 0){
            int curTotal = totals[lastPos];
            int curNumCards = numCards[lastPos];
            bool curSoft = soft[lastPos];
            totals[lastPos] = prevTotal;
            numCards[lastPos] = prevNumCards;
            soft[lastPos] = wasSoft;

            bool shouldUndo = false;
            for(int i = 0; i < 4; i++){
                if (totals[i] + curCard == 21 || (totals[i]+curCard < 21 && numCards[i] == 4) || curCard == 0 || 
                    (totals[i] + curCard == 11 && (soft[i] || curCard == 1))){
                    shouldUndo = true;
                    break;
                }
            }

            totals[lastPos] = curTotal;
            numCards[lastPos] = curNumCards;
            soft[lastPos] = curSoft;
            if (shouldUndo){
                justUndid = true;
                canUndo = false;
                return true;
            }
        } else if (prevStreak == 0 && streak == 0 && canUndo && lastPos >= 0 && curCard >= 2 && prevCard >= 2){
            int curTotal = totals[lastPos];
            int curNumCards = numCards[lastPos];
            bool curSoft = soft[lastPos];
            if (curTotal + curCard == 21 || (curTotal + curCard == 11 && (curSoft || curCard == 1))){

            } else {
                totals[lastPos] = prevTotal;
                numCards[lastPos] = prevNumCards;
                soft[lastPos] = wasSoft;

                bool shouldUndo = false;
                for(int i = 0; i < 4; i++){
                    if (totals[i] + curCard + prevCard == 21 || 
                        (totals[i] + curCard + prevCard == 11 && soft[i])){
                        shouldUndo = true;
                        break;
                    }
                }

                totals[lastPos] = curTotal;
                numCards[lastPos] = curNumCards;
                soft[lastPos] = curSoft;
                if (shouldUndo){
                    justUndid = true;
                    canUndo = false;
                    return true;
                }
            }
        }

        if (curCard > 0){
            int outs = 0;
            int cardsNeeded[11] = {0};
            int numUnder10 = 0;
            for(int i = 0; i < 4; i++){
                if (totals[i] > 11){
                    outs += left[21 - totals[i]];
                    cardsNeeded[21 - totals[i]]++;
                } else if (totals[i] == 11){
                    outs += left[10];
                    cardsNeeded[10]++;
                } else {
                    if (left[10] >= cardsLeft / 4){
                        if (totals[i] == 0){
                            outs += left[1];
                        } else {
                            outs += left[11 - totals[i]];
                        }
                    }
                }
                if (totals[i] < 10){
                    numUnder10++;
                }
            }
            // int totalsUnder10[10] = {0};
            // for(int i = 0; i < 4; i++){
            //     if (totals[i] < 10){
            //         totalsUnder10[totals[i]]++;
            //     }
            // }

            int undoTotal = totals[lastPos];
            int undoNumCards = numCards[lastPos];
            for(int i = 0; i < 4; i++){
                if (numUnder10 > 0 && totals[i] + curCard == 20 && numCards[i] < 4){ // don't make 20
                    continue;
                }
                if (nextCard != -1 && lastPos == i && undoCounter == 2) continue; // just undid, don't make same move
                if (nextCard != -1 && lastPos >= 0 && undoCounter == 2 && undoTotal == totals[i] && undoNumCards == numCards[i] && soft[lastPos] == soft[i]) continue; // just undid, don't make same move
                int total = totals[i] + curCard;
                int score = 0;
                if (total == 21){
                    score = 160;
                    if (numCards[i] >= 4){
                        score += 50;
                    }
                } else if (total == 11 && (soft[i] || curCard == 1)){
                    score = 100;
                    if (numCards[i] >= 4){
                        score += 50;
                    }
                } else if (total > 21){
                    score = -100;
                } else if (numCards[i] >= 4){
                    score = 60;
                } else {
                    
                    if (streak == 0 && nextCard != -1 && 
                        (total + nextCard == 21 || (total + nextCard == 11 && soft[i]))){
                        score = 85;
                    } else if (totals[i] > 11) {
                        score = 0;
                        if (cardsNeeded[21 - totals[i]] == 1){
                            score -= left[21 - total];
                        }
                        if (cardsNeeded[21 - total] == 0){
                            score += left[21 - total];
                        }
                    } else if (totals[i] == 11){
                        score = - left[10] + left[21 - total];
                    } else {
                        if (left[10] >= cardsLeft / 4){
                            if (totals[i] == 0){
                                score = outs - left[1];
                            } else {
                                score = outs - left[11 - totals[i]];
                            }
                        }
                        if (total > 11){
                            score += left[21 - total];
                        } else if (total == 11){
                            score += left[10];
                        } else {
                            if (totals[i] > 1){
                                score -= left[11 - totals[i]];
                            }
                            score += left[11 - total];
                        }
                    }
                }
                if (score > bestScore){
                    bestScore = score;
                    move = i;
                }
            }
        } else if (curCard == 0){
            for(int i = 0; i < 4; i++){
                if (totals[i] % 10 != 1 && !(nextCard != -1 && 
                    (totals[i] + nextCard == 21 || (totals[i] + nextCard == 11 && (soft[i] || nextCard == 1)) ||
                     (totals[i] + nextCard < 21 && numCards[i] == 4)))){
                    move = i;
                    break;
                }
            }
            if (move == -1){
                move = 0;
            }
            bestScore = 90;
        }
        int numSpaces = 0;
        int num11 = 0;
        int numUnder11 = 0;
        for(int i = 0; i < 4; i++){
            if (totals[i] == 0) numSpaces++;
            if (totals[i] % 10 == 1) num11++;
            if (totals[i] < 11) numUnder11++;
        }
        curMove = move;
        return true;
    }

    void getRGB(uint8 * pixels, int width, int height, int bpp, int x, int y, uint8 &r, uint8 &g, uint8 &b){

        int refWidth = 714;
        int refHeight = 1056;
        double xpct = (double)x / (double)refWidth;
        double ypct = (double)y / (double)refHeight;
        x = (int)llround(xpct * (double)width);
        y = (int)llround(ypct * (double)height);

        // Map screen-space coords (x,y) into the provided pixel buffer.
        const int px = (int)llround((x - captureOffsetX) * captureScaleX);
        const int py = (int)llround((y - captureOffsetY) * captureScaleY);

        if (px < 0 || py < 0 || px >= width || py >= height || bpp <= 0) {
            r = g = b = 0;
            return;
        }

        uint8 * p = pixels + (py * width + px) * bpp;
        r = (uint8)(p[2]);
        g = (uint8)(p[1]);
        b = (uint8)(p[0]);
    }


    State * fromPixels(uint8 *pixels, int width, int height, int bpp, int prevCard){

        int cardExistx1 = 401;
        int cardExisty = 793;
        int cardExistx2 = 482;

        uint8 r,g,b;

        getRGB(pixels, width, height, bpp, cardExistx1, cardExisty, r, g, b);
        if (r < 220 || g < 220 || b < 220){
            return NULL;
        }
        getRGB(pixels, width, height, bpp, cardExistx2, cardExisty, r, g, b);
        if (r < 220 || g < 220 || b < 220){
            return NULL;
        }

        // detect dark around the card 
        int cardNotExistx = 385;

        getRGB(pixels, width, height, bpp, cardNotExistx, cardExisty, r, g, b);
        if (r > 200 && g > 200 && b > 200){
            return NULL;
        }

        int suit = -1;

        int suitx1 = 460;
        int suity1 = 744;
        getRGB(pixels, width, height, bpp, suitx1, suity1, r, g, b);

        bool leftRed = r > 180 && g < 60 && b < 50;

        int suitx2 = 477;
        int suity2 = 744;
        getRGB(pixels, width, height, bpp, suitx2, suity2, r, g, b);

        bool rightRed = r > 180 && g < 60 && b < 50;

        if (rightRed){
            suit = 0; // heart
        }

        int suitx3 = 468;
        int suity3 = 742;
        getRGB(pixels, width, height, bpp, suitx3, suity3, r, g, b);

        if (r > 180 && g < 60 && b < 60){
            if (suit == -1){
                suit = 1; // diamond
            }
        }
        bool leftBlack = false;
        bool rightBlack = false;
        if (r < 95 && g < 95 && b < 95){
            int suitx4 = 463;
            int suity4 = 742;
            getRGB(pixels, width, height, bpp, suitx4, suity4, r, g, b);

            bool leftBlack = r < 105 && g < 105 && b < 105;

            int suitx5 = 472;
            int suity5 = 742;
            getRGB(pixels, width, height, bpp, suitx5, suity5, r, g, b);

            bool rightBlack = r < 105 && g < 105 && b < 105;

            suit = leftBlack && rightBlack ? 2 : 3; // club or spade
            if (leftBlack != rightBlack) suit = -1;
        }

        int rank = -1;

        vector<vector<pair<int, int>>> filled = {
            {{440,786},{440,821},{419,834},{465,835}}, // A
            {{441,785},{425,796},{454,796},{448,813},{443,833},{459,833}}, // 2
            {{442,785},{441,809},{441,834},{425,833}}, // 3
            {{450,786},{433,800},{420,821},{464,820},{452,835},{439,822}}, // 4
            {{426,784},{457,784},{442,784},{428,797},{428,809},{442,835}}, // 5
            {{442,785},{441,809},{441,834}, {431,809},{424,820}}, // 6
            {{422,786},{441,786},{461,786},{451,800},{443,814},{439,835}}, // 7
            {{442,785},{441,809},{441,834}, {431,809},{424,820},{460,799},{424,819}}, // 8
            {{442,785},{441,809},{441,834}, {431,809},{459,796}}, // 9
            {{410,794},{424,837}}, // 10
            {{457,785},{441,834},{426,820}}, // J
            {{464,845},{442,786}}, // Q
            {{424,784},{457,785},{437,810},{424,834},{460,834}}, // K
            {{457,785},{441,834},{426,820}} // J
        };
        vector<vector<pair<int, int>>> empty = {
            {{440,810},{440,834}}, // A
            {{425,807},{459,820},{440,799}}, // 2
            {{441,796},{441,822},{431,809}}, // 3
            {{440,810}}, // 4
            {{459,796},{443,796},{424,819}}, // 5
            {{441,796},{441,822},{460,799}}, // 6
            {{427,798},{426,824}}, // 7
            {{441,796},{441,822}}, // 8
            {{441,796},{441,822},{424,820}}, // 9
            {{458,809}}, // 10
            {{424,806},{425,786},{440,818}}, // J
            {{441,809}}, // Q
            {{410,794},{439,834},{439,784}}, // K
            {{424,806},{425,786},{440,818}} // J
        };
        vector<int> ranks = {0,1,2,3,4,5,6,7,8,9,10,11,12,10};
        vector<bool> good = vector<bool>(filled.size(), true);
        vector<int> bad;
        for(int i = 0; i < filled.size(); i++){
            for(int j = 0; j < filled[i].size(); j++){
                getRGB(pixels, width, height, bpp, filled[i][j].first, filled[i][j].second, r, g, b);
                if (r < 95 && g < 95 && b < 95){ // black

                } else if (r > 70 && r < 100 && g > 70 && g < 100 && b > 70 && b < 100){ // black

                } else if (r > 85 && r < 110 && g > 85 && g < 110 && b > 85 && b < 110){ // black

                } else if (r > 70 && r < 110 && g < 40 && b > 90 && b < 120){ // purple
                    
                } else if (r > 140 && g < 60 && b < 80){ // red

                } else if (r > 130 && r < 180 && g < 50 && b > 10 && b < 80){ // red
                    
                } else if (r > 170 && r < 220 && g < 45 && b > 70 && b < 120){
                    
                } else if (r > 170 && r < 200 && g < 25 && b > 20 && b < 70){
                    
                } else {
                    good[i] = false;
                    if (i == 8) {
                        //bad.push_back(j);
                        //cout << (int)r << " " << (int)g << " " << (int)b << endl;
                    }
                    break;
                }
            }
        }
        for(int i = 0; i < empty.size(); i++){
            if (!good[i]) continue;
            for(int j = 0; j < empty[i].size(); j++){
                getRGB(pixels, width, height, bpp, empty[i][j].first, empty[i][j].second, r, g, b);
                if (r > 180 && g > 180 && b > 180){ // white

                } else {
                    good[i] = false;
                    if (i == 1) bad.push_back(j);
                    break;
                }
            }
        }

        bool hasGood = false;
        for(int i = 0; i < good.size(); i++){
            if (good[i]) {
                hasGood = true;
            }
            if (good[i] && suit >= 0 && ranks[i] * 4 + suit != prevCard){
                int card = ranks[i];
                card = card * 4 + suit;
                return sampleState(card);
            }
        }
        if (suit < 0){
            //cerr << "bad suit " << leftRed << " " << rightRed << " " << leftBlack << " " << rightBlack << endl;
        }
        if (!hasGood){
            // cerr << "no good" << endl;
            // for(int i = 0; i < bad.size(); i++){
            //     cerr << bad[i] << " ";
            // }
        } 
        return NULL;
    }
};