#pragma once

#include <vector>
#include <iostream>
#include <cstdlib>
#include <cmath>

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
        int cardExistx1 = 1667;
        int cardExisty = 911;
        int cardExistx2 = 1740;

        uint8 r,g,b;
        getRGB(pixels, width, height, bpp, cardExistx1, cardExisty, r, g, b);
        if (r < 220 || g < 220 || b < 220){
            return NULL;
        }
        getRGB(pixels, width, height, bpp, cardExistx2, cardExisty, r, g, b);
        if (r < 220 || g < 220 || b < 220){
            return NULL;
        }

        int suit = -1;

        int suitx1 = 1663;
        int suity1 = 855;
        getRGB(pixels, width, height, bpp, suitx1, suity1, r, g, b);

        bool leftRed = r > 140 && r < 200 && g < 60 && b > 50 && b < 110;

        int suitx2 = 1677;
        int suity2 = 855;
        getRGB(pixels, width, height, bpp, suitx2, suity2, r, g, b);

        bool rightRed = r > 140 && r < 200 && g < 60 && b > 50 && b < 110;

        if (rightRed){
            suit = 0; // heart
        }

        int suitx3 = 1670;
        int suity3 = 862;
        getRGB(pixels, width, height, bpp, suitx3, suity3, r, g, b);

        if (r > 140 && r < 200 && g < 60 && b > 50 && b < 110){
            if (suit == -1){
                suit = 1; // diamond
            }
        }
        bool leftBlack = false;
        bool rightBlack = false;
        if (r < 95 && g < 95 && b < 95){
            int suitx4 = 1668;
            int suity4 = 854;
            getRGB(pixels, width, height, bpp, suitx4, suity4, r, g, b);

            bool leftBlack = r < 105 && g < 105 && b < 105;

            int suitx5 = 1674;
            int suity5 = 854;
            getRGB(pixels, width, height, bpp, suitx5, suity5, r, g, b);

            bool rightBlack = r < 105 && g < 105 && b < 105;

            suit = leftBlack && rightBlack ? 2 : 3; // club or spade
            if (leftBlack != rightBlack) suit = -1;
        }

        int rank = -1;

        vector<vector<int>> filledx = {
            {1702,1689,1715,1686,1701,1719,1682,1726}, // A
            {1686,1699,1719,1699,1684,1722}, // 2
            {1685,1700,1718,1707,1693,1717,1683,1701}, // 3
            {1710,1714,1714,1728,1680,1692}, // 4
            {1689,1705,1718,1689,1690,1703,1716,1718,1703,1686}, // 5
            {1719,1708,1688,1686,1691,1706,1719,1706,1688}, // 6
            {1685,1697,1720,1710,1702,1695,1705}, // 7
            {1687,1702,1718,1702,1718,1686,1719,1703}, // 8
            {1701,1685,1717,1687,1703,1719,1718,1705,1688}, // 9
            {1672,1684,1684,1735}, // 10
            {1705,1705,1705,1695,1686}, // J
            {1686,1701,1720,1681,1724,1727}, // Q
            {1685,1721,1687,1701,1686,1723}, // K
            {1705,1705,1705,1695,1690} // J
        };
        vector<vector<int>> filledy = {
            {879,898,898,915,914,915,929,929}, // A
            {881,878,891,908,927,929}, // 2
            {879,877,888,902,902,926,926,928}, // 3
            {878,915,929,915,914,894}, // 4
            {878,878,878,890,902,901,904,921,929,926}, // 5
            {879,876,883,906,925,928,916,900,914}, // 6
            {878,878,878,901,915,929}, // 7
            {888,877,888,902,897,917,917,930}, // 8
            {877,891,891,902,905,903,917,929,927}, // 9
            {887,884,923,903}, // 10
            {894,910,932,936,936}, // J
            {885,879,885,902,902,940}, // Q
            {880,882,902,903,927,928}, // K
            {894,910,932,932,930}, // J
        };
        vector<vector<int>> emptyx = {
            {1703,1703}, // A
            {1698,1687}, // 2
            {1699,1698,1680,1680}, // 3
            {1701}, // 4
            {1688,1700,1705,1720}, // 5
            {1703,1720,1705}, // 6
            {1685,1694,1684,1722}, // 7
            {1703,1703}, // 8
            {1699,1688,1702}, // 9
            {1717,1717,1717}, // 10
            {1683,1683,1690,1719}, // J
            {1703}, // Q
            {1702,1722,1702}, // K
            {1692,1692,1692}, // J
        };
        vector<vector<int>> emptyy = {
            {928,902}, // A
            {892,901}, // 2
            {915,889,892,912}, // 3
            {904}, // 4
            {914,916,890,891}, // 5
            {914,890,889}, // 6
            {912,895,895,929}, // 7
            {917,889}, // 8
            {918,916,892}, // 9
            {893,904,913}, // 10
            {917,904,901,907}, // J
            {903}, // Q
            {880,904,930}, // K
            {911,897,879}, // J
        };
        vector<int> ranks = {0,1,2,3,4,5,6,7,8,9,10,11,12,10};
        vector<bool> good = vector<bool>(filledx.size(), true);
        vector<int> bad;
        for(int i = 0; i < filledx.size(); i++){
            for(int j = 0; j < filledx[i].size(); j++){
                getRGB(pixels, width, height, bpp, filledx[i][j], filledy[i][j], r, g, b);
                if (r < 95 && g < 95 && b < 95){ // black

                } else if (r > 70 && r < 100 && g > 70 && g < 100 && b > 70 && b < 100){ // black

                } else if (r > 85 && r < 110 && g > 85 && g < 110 && b > 85 && b < 110){ // black

                } else if (r > 70 && r < 110 && g < 40 && b > 90 && b < 120){ // purple
                    
                } else if (r > 140 && r < 200 && g < 60 && b > 50 && b < 110){ // red

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
        for(int i = 0; i < emptyx.size(); i++){
            if (!good[i]) continue;
            for(int j = 0; j < emptyx[i].size(); j++){
                getRGB(pixels, width, height, bpp, emptyx[i][j], emptyy[i][j], r, g, b);
                if (r > 220 && g > 220 && b > 220){ // white

                } else {
                    good[i] = false;
                    if (i == 1) bad.push_back(j);
                    break;
                }
            }
        }

        bool hasGood = false;
        for(int i = 0; i < good.size(); i++){
            if (good[i]) hasGood = true;
            if (good[i] && suit >= 0 && ranks[i] * 4 + suit != prevCard){
                int card = ranks[i];
                if (card == 10 && suit == 2) suit = 3; // wild card is always spades
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