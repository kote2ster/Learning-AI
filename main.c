/** @file main.c  */
/** [TOC]
 * @mainpage AI from database
 * @section intro_sec Introduction
 *  @author    Akos Kote
 *  @version   1.0
 *  @date      Last Modification: 2015.04.28.
 *
 * [GitHub Project](https://github.com/kote2ster/Learning-AI "GitHub Project")
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libtcod.h>

#define BLOCK_WDTH 45 /**< Width of a block */
#define CON_Y 80      /**< Console height */
#define FPS 60    /**< FPS of LIBTCOD */
#define MAX_MEM_ALLOC 1000000 /**< Max allowed memory allocation (in byte) */
#define EASY /**< You can define EASY,MEDIUM,HARD */
/*------DIFFICULTY SETTINGS------*/
#ifdef EASY /* 0.3sec/move,at least LearnedAISize/10 unkown,below 7 digwidth 20% chance of shortening digwidth */
#define WAIT 300 /**< Waiting time between main cycles (in ms) */
#define UNKNOWN_FOR_AI LearnedAISize/10+rand()%(LearnedAISize*8/10+1) /**< Number of situations unknown for AI */
#define ALTER_DIGWIDTH_BELOW 7 /**< Alter next digwidth chance below this width value */
#define CHANCE_DIV 2 /** chanceDiv*10% chance for shortening */
#endif // EASY
#ifdef MEDIUM /* 0.2sec/move,at least LearnedAISize/20 unkown,below 5 digwidth 30% chance of shortening digwidth */
#define WAIT 200 /**< Waiting time between main cycles (in ms) */
#define UNKNOWN_FOR_AI LearnedAISize/20+rand()%(LearnedAISize*10/20+1) /**< Number of situations unknown for AI */
#define ALTER_DIGWIDTH_BELOW 5 /**< Alter next digwidth chance below this width value */
#define CHANCE_DIV 3 /** chanceDiv*10% chance for shortening */
#endif // MEDIUM
#ifdef HARD /* 0.1sec/move,at least LearnedAISize/20 unkown,below 5 digwidth 60% chance of shortening digwidth */
#define WAIT 100 /**< Waiting time between main cycles (in ms) */
#define UNKNOWN_FOR_AI LearnedAISize/20+rand()%(LearnedAISize*3/20+1) /**< Number of situations unknown for AI */
#define ALTER_DIGWIDTH_BELOW 5 /**< Alter next digwidth chance below this width value */
#define CHANCE_DIV 6 /** chanceDiv*10% chance for shortening */
#endif // HARD
/*-------------------------------*/
unsigned char Level[2][CON_Y]; /**< Stores hole starting position and width of hole in each lines */
unsigned char **LearnedAI; /**< Learned positions, relevant level sections */
int LearnedAISize; /**< Size of @see LearnedAI */
int UnknownForAI; /**< Number of unknown sections for AI @see LearnedAI */
TCOD_key_t key; /**< Key event handler */
uint32 Timer;  /**< Time handler */
int AIpos,PLYpos; /**< AI and player current position */

typedef enum {HOLEDIG=0,WDTHDIG=1} INDEX; /**< Array index enum */
enum {NONE,AICRASHED,PLYCRASHED} CRASHED; /**< Indicator if someone crashed */

/** Wait function until key event or msec
 * @param msec [in] wait for millisec
 */
void Wait(uint32 msec) {
    TCOD_event_t event; //event handler
    static int speedup=0;
    TCOD_key_t localkey;
    key.vk=TCODK_NONE; //Reset keypress
    if(speedup) msec=0;
    do {
        TCOD_sys_sleep_milli(1000/FPS); //To prevent extensive cpu usage
        event=TCOD_sys_check_for_event(TCOD_EVENT_KEY,&localkey,NULL);
        if(event==TCOD_EVENT_KEY_PRESS&&localkey.vk==TCODK_UP) {speedup=1; msec=0;} //Speed up
        else if(event==TCOD_EVENT_KEY_RELEASE&&localkey.vk==TCODK_UP) speedup=0; //Reset
        if(event==TCOD_EVENT_KEY_PRESS&&(localkey.vk==TCODK_LEFT||localkey.vk==TCODK_RIGHT||localkey.vk==TCODK_ESCAPE)) {speedup=0; key.vk=localkey.vk;} //Lock keypress
    } while(localkey.vk!=TCODK_ESCAPE&&Timer+msec>TCOD_sys_elapsed_milli()&&!TCOD_console_is_window_closed());
}

/** Initialize array with spaces */
void IniLevelArray() {
    int j;
    for(j=0; j<CON_Y; j++) { //Level vertical
        Level[HOLEDIG][j]=0; //Hole starting position and width of hole
        Level[WDTHDIG][j]=0;
    }
}

void InitRandomLevel() {
    int j,dice,prevhole,digwidth,dir=0; //digwidth: previous hole width; dir: direction (-1:left,0:same,1:right)
    IniLevelArray(); //resetting level array data
    prevhole=rand()%BLOCK_WDTH; //Pick a starting hole
    digwidth=rand()%BLOCK_WDTH; //Pick a starting width
    CRASHED=NONE;
    for(j=0; j<CON_Y/2; j++) { //Level vertical, specified to be able to manage every situation
        Level[HOLEDIG][j]=prevhole; //Hole stating point marks
        Level[WDTHDIG][j]=digwidth; //Width of a hole
        dir=rand()%3-1; //pick a random dig direction in every line (-1:left +0:same +1:right)
        if(prevhole+dir<0) dir=rand()%2; //Digging to out of boundary is forbidden
        else if(prevhole+dir>=BLOCK_WDTH) dir=rand()%2-1;
        prevhole=prevhole+dir; //New starting dig point
        if(digwidth<ALTER_DIGWIDTH_BELOW) { //Check if width is small
            dice=rand()%10;
            if(dice<CHANCE_DIV) digwidth=digwidth-rand()%5; //CHANCE_DIV*10% chance to shorten width by -4-3-2-1+0
            else digwidth=digwidth+rand()%5; //1-CHANCE_DIV*10% chance to broaden width by +0+1+2+3+4
        } else digwidth=digwidth+rand()%9-4; //Change digging width by -4-3-2-1+0+1+2+3+4
        if(digwidth<0) digwidth=0; //Check if digging width is out of boundary
        else if(digwidth>BLOCK_WDTH-1) digwidth=BLOCK_WDTH-1;
    }
    for(j=CON_Y/2; j<CON_Y; j++) {
        Level[HOLEDIG][j]=prevhole;
        Level[WDTHDIG][j]=BLOCK_WDTH-1;
    }
    //Set AI and player positions
    AIpos=prevhole;
    PLYpos=prevhole+BLOCK_WDTH+1;
}

void ScrollLevels() {
    int j,dice,prevhole,digwidth=0,dir=0; //digwidth: previous hole width; dir: direction (-1:left,0:same,1:right)
    //Shift lines
    for(j=CON_Y-2; j>=0; j--) {
        Level[HOLEDIG][j+1]=Level[HOLEDIG][j];
        Level[WDTHDIG][j+1]=Level[WDTHDIG][j];
    }
    //Randomize new first line
    prevhole=Level[HOLEDIG][0];
    digwidth=Level[WDTHDIG][0];
    dir=rand()%3-1; //pick a random dig direction in every line (-1:left +0:same +1:right)
    if(prevhole+dir<0) dir=rand()%2; //Digging to out of boundary is forbidden
    else if(prevhole+dir>=BLOCK_WDTH) dir=rand()%2-1;
    prevhole=prevhole+dir; //New starting dig point
    if(digwidth<ALTER_DIGWIDTH_BELOW) { //Check if width is small
        dice=rand()%10;
        if(dice<CHANCE_DIV) digwidth=digwidth-rand()%5; //CHANCE_DIV*10% chance to shorten width by -4-3-2-1+0
        else digwidth=digwidth+rand()%5; //1-CHANCE_DIV*10% chance to broaden width by +0+1+2+3+4
    } else digwidth=digwidth+rand()%9-4; //Change digging width by -4-3-2-1+0+1+2+3+4
    if(digwidth<0) digwidth=0; //Check if digging width is out of boundary
    else if(digwidth>BLOCK_WDTH-1) digwidth=BLOCK_WDTH-1;
    Level[HOLEDIG][0]=prevhole; //Hole stating point marks
    Level[WDTHDIG][0]=digwidth; //Store digwidth
}
/** This function "teaches" AI to solve positions */
void TeachAI() {
    int i,j,k,starthole,nexthole,startwidth,digwidth,dir=0; //digwidth: previous hole width; dir: direction (-1:left,0:same,1:right)
    int *omitIndx=NULL,skipSect=0; //Indexes list which are omitted, after random gen
    LearnedAISize=0;
    //Count all possible level sections, two lines are enough
    for(starthole=0; starthole<BLOCK_WDTH; starthole++) { //Pick a starting hole
        for(startwidth=0; startwidth<BLOCK_WDTH; startwidth++) { //Pick a starting width
            for(digwidth=startwidth-4; digwidth<=startwidth+4; digwidth++) { //Change digging width for the next line by -4-3-2-1+0+1+2+3+4
                if(digwidth<0||digwidth>=BLOCK_WDTH) continue; //Check if digging width is out of boundary
                for(dir=-1; dir<2; dir++) { //Pick a dig direction for the next line (-1:left +0:same +1:right)
                    nexthole=starthole+dir; //New starting dig point
                    if(nexthole<0||nexthole>=BLOCK_WDTH) continue; //Digging to out of boundary is forbidden

                    LearnedAISize++; //Unique sector, counting...
                }
            }
        }
    }
    UnknownForAI=UNKNOWN_FOR_AI; //At most UNKNOWN_FOR_AI index will be omitted (these situations will be unknown by the AI)
    if(UnknownForAI>LearnedAISize-1) UnknownForAI=LearnedAISize-1;
    if(UnknownForAI!=0) {
        omitIndx = (int*)calloc(LearnedAISize,sizeof(int));
        for(i=0; i<LearnedAISize; i++) omitIndx[i]=i; //Fill with index numbers
    }
    for(i=LearnedAISize; i>LearnedAISize-UnknownForAI; i--) { //Max indexes from random will choose
        j=rand()%i; //Randomize an index for omitIndx
        k=omitIndx[j]; //Store value
        omitIndx[j]=omitIndx[i-1]; //Swap values
        omitIndx[i-1]=k; //Store value at the end of the array
    }
    LearnedAISize=LearnedAISize-UnknownForAI;
    if(LearnedAISize*2*2*sizeof(char)>MAX_MEM_ALLOC) {
        if(omitIndx!=NULL) free(omitIndx);
        TCOD_console_print(NULL,0,0,"Too big learned array size, reduce CON_Y");
        TCOD_console_flush();
        TCOD_console_wait_for_keypress(1);
        exit(-1);
    }
    LearnedAI = (unsigned char**)calloc(LearnedAISize,sizeof(unsigned char*)); //2D array init
    for(i=0; i<LearnedAISize; i++) LearnedAI[i] = (unsigned char*)calloc(2*2, sizeof(unsigned char)); //2 lines of Hole starting position and width of hole
    for(j=0; j<LearnedAISize; j++) {
        LearnedAI[j][  HOLEDIG]=LearnedAI[j][  WDTHDIG]=0; //Initialize array, first line
        LearnedAI[j][2+HOLEDIG]=LearnedAI[j][2+WDTHDIG]=0; //Initialize array, second line
    }
    //Generate all possible level sections
    j=k=0;
    for(starthole=0; starthole<BLOCK_WDTH; starthole++) { //Pick a starting hole
        for(startwidth=0; startwidth<BLOCK_WDTH; startwidth++) { //Pick a starting width
            for(digwidth=startwidth-4; digwidth<=startwidth+4; digwidth++) { //Change digging width for the next line by -4-3-2-1+0+1+2+3+4
                if(digwidth<0||digwidth>=BLOCK_WDTH) continue; //Check if digging width is out of boundary
                for(dir=-1; dir<2; dir++) { //Pick a dig direction for the next line (-1:left +0:same +1:right)
                    nexthole=starthole+dir; //New starting dig point
                    if(nexthole<0||nexthole>=BLOCK_WDTH) continue; //Digging to out of boundary is forbidden

                    for(i=LearnedAISize+UnknownForAI; i>LearnedAISize; i--)
                        if(k==omitIndx[i-1]) skipSect=1; //Skip sector?

                    if(!skipSect) {
                        LearnedAI[j][  HOLEDIG]=starthole; //Level section, first line
                        LearnedAI[j][  WDTHDIG]=startwidth;
                        LearnedAI[j][2+HOLEDIG]=nexthole; //Level section, second line
                        LearnedAI[j][2+WDTHDIG]=digwidth;
                        j++; //Next sector
                    }
                    skipSect=0;
                    k++; //Next iteration
                }
            }
        }
    }
    if(omitIndx!=NULL) free(omitIndx);
}
/** In this function AI and player move */
void Move() {
    int j,match=1;
    switch(key.vk) {
    case TCODK_LEFT:
        if(PLYpos-1>BLOCK_WDTH) PLYpos--;
        break;
    case TCODK_RIGHT:
        if(PLYpos+1<BLOCK_WDTH*2+1) PLYpos++;
        break;
    default:
        break;
    }
    //Search for exact level section match, for AI
    for(j=0; j<LearnedAISize; j++) {
        match=0;
        if(     LearnedAI[j][  HOLEDIG]==Level[HOLEDIG][CON_Y-2]&& //First line
                LearnedAI[j][  WDTHDIG]==Level[WDTHDIG][CON_Y-2]&&
                LearnedAI[j][2+HOLEDIG]==Level[HOLEDIG][CON_Y-1]&& //Second line
                LearnedAI[j][2+WDTHDIG]==Level[WDTHDIG][CON_Y-1]) {
            match=1;
            break;
        }
    }
    if(match&&LearnedAI[j][2+HOLEDIG]==AIpos) AIpos+=(LearnedAI[j][HOLEDIG]-LearnedAI[j][2+HOLEDIG]); //If found section make move, first line hole - second line hole
    else if(match&&AIpos<LearnedAI[j][2+HOLEDIG]) AIpos=AIpos+rand()%2;   //if section is known and pos is smaller, then make random move forward or right
    else if(match&&AIpos>LearnedAI[j][2+HOLEDIG]) AIpos=AIpos+rand()%2-1; //if section is known and pos is bigger, then make random move forward or left
    else {
        if(AIpos==Level[HOLEDIG][CON_Y-1]) { //If in position
            AIpos=AIpos+rand()%3-1; //make random move
            if(AIpos<0) AIpos=0; //Moving to out of boundary is forbidden
            else if(AIpos>=BLOCK_WDTH) AIpos=BLOCK_WDTH-1;
            if(AIpos==Level[HOLEDIG][CON_Y-2]) { //If random move finds the next line's hole, then learn this section
                UnknownForAI--;
                LearnedAISize++;
                LearnedAI = (unsigned char**)realloc(LearnedAI,LearnedAISize*sizeof(unsigned char*));
                LearnedAI[LearnedAISize-1] = (unsigned char*)calloc(2*2, sizeof(unsigned char));
                LearnedAI[LearnedAISize-1][  HOLEDIG] = Level[HOLEDIG][CON_Y-2];
                LearnedAI[LearnedAISize-1][  WDTHDIG] = Level[WDTHDIG][CON_Y-2];
                LearnedAI[LearnedAISize-1][2+HOLEDIG] = Level[HOLEDIG][CON_Y-1];
                LearnedAI[LearnedAISize-1][2+WDTHDIG] = Level[WDTHDIG][CON_Y-1];
            }
        } else AIpos=AIpos+rand()%3-1; //make random move
    }
    if(AIpos<0) AIpos=0; //Moving to out of boundary is forbidden
    else if(AIpos>=BLOCK_WDTH) AIpos=BLOCK_WDTH-1;
}
/** This function is for crash animation */
void CrashAnimation() {
    int i,j,k,pos;
    switch(CRASHED) {
    case AICRASHED:
        pos=AIpos;
        TCOD_console_set_default_foreground(NULL,TCOD_red);
        TCOD_console_print_ex(NULL,BLOCK_WDTH,CON_Y/2,TCOD_BKGND_DEFAULT,TCOD_CENTER,"PLAYER WON");
        break;
    case PLYCRASHED:
        pos=PLYpos;
        TCOD_console_set_default_foreground(NULL,TCOD_yellow);
        TCOD_console_print_ex(NULL,BLOCK_WDTH,CON_Y/2,TCOD_BKGND_DEFAULT,TCOD_CENTER,"AI WON");
        break;
    default:
        pos=0;
    }
    TCOD_console_set_default_foreground(NULL,TCOD_red);
    for(k=0; k<3; k++) { //3 iteration (state)
        for(i=pos-1; i<=pos+1; i++) {
            for(j=CON_Y-2; j<=CON_Y-1; j++) {
                if(i>=0&&i<=BLOCK_WDTH*2) {
                    switch(k) {
                    case 0:
                        TCOD_console_put_char(NULL,i,j,'-',TCOD_BKGND_DEFAULT);
                        break;
                    case 1:
                        TCOD_console_put_char(NULL,i,j,'*',TCOD_BKGND_DEFAULT);
                        break;
                    case 2:
                        TCOD_console_put_char(NULL,i,j,'#',TCOD_BKGND_DEFAULT);
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        TCOD_console_flush();
        Timer=TCOD_sys_elapsed_milli(); //Timing
        Wait(250);
        if(key.vk==TCODK_ESCAPE||TCOD_console_is_window_closed()) break;
    }
    TCOD_console_set_default_foreground(NULL,TCOD_white);
}
/** Separator between two blocks */
void PrintIndicators() {
    if(TCOD_sys_elapsed_milli()<6000) {
        TCOD_console_set_default_foreground(NULL,TCOD_yellow);
        TCOD_console_print_ex(NULL,BLOCK_WDTH-1,0,TCOD_BKGND_DEFAULT,TCOD_RIGHT,"AI");
        TCOD_console_set_default_foreground(NULL,TCOD_red);
        TCOD_console_print_ex(NULL,BLOCK_WDTH*2,0,TCOD_BKGND_DEFAULT,TCOD_RIGHT,"PLAYER");
        TCOD_console_set_default_foreground(NULL,TCOD_white);
    }
    TCOD_console_set_default_foreground(NULL,TCOD_white);
    TCOD_console_print(NULL,0,0,"Unknown for AI: %d",UnknownForAI);
    TCOD_console_print(NULL,0,1,"Speed: %.0fFPS",1000.0/WAIT);
    TCOD_console_set_default_foreground(NULL,TCOD_white);
}
/** Separator between two blocks */
void PrintSeparator() {
    int i;
    for(i=0; i<CON_Y; i++) {
        TCOD_console_put_char(NULL,BLOCK_WDTH,i,TCOD_CHAR_VLINE,TCOD_BKGND_DEFAULT);
    }
}
/** Printing levels left and right are the same */
void PrintLevels() {
    int i,j,printChar=' ';
    TCOD_console_clear(NULL); //Clear Console
    PrintSeparator();
    for(j=0; j<CON_Y; j++) { //Level vertical
        for(i=0; i<BLOCK_WDTH; i++) { //Level horizontal

            if((i<Level[HOLEDIG][j]-Level[WDTHDIG][j]/2||i>Level[HOLEDIG][j]+(Level[WDTHDIG][j]+1)/2)&&(j==0||(j!=0&&i!=Level[HOLEDIG][j-1]))) printChar=TCOD_CHAR_BLOCK3;
//            else if(i==Level[HOLEDIG][j]) printChar='*';
            else printChar=' ';

            TCOD_console_put_char(NULL,i,j,printChar,TCOD_BKGND_DEFAULT); //Left level
            TCOD_console_put_char(NULL,i+BLOCK_WDTH+1,j,printChar,TCOD_BKGND_DEFAULT); //Right level
        }
    }
    TCOD_console_set_default_foreground(NULL,TCOD_yellow);
    if(TCOD_console_get_char(NULL,AIpos,CON_Y-1)!=TCOD_CHAR_BLOCK3) TCOD_console_put_char(NULL,AIpos,CON_Y-1,'@',TCOD_BKGND_DEFAULT); //Place AI
    else CRASHED=AICRASHED; //AI Crashed!
    for(i=CON_Y-2; i>=0&&i>=CON_Y-8; i--) {
        if(TCOD_console_get_char(NULL,PLYpos,i)==TCOD_CHAR_BLOCK3) {
            TCOD_console_set_default_foreground(NULL,TCOD_dark_red);
            TCOD_console_put_char(NULL,PLYpos,i,TCOD_CHAR_BLOCK3,TCOD_BKGND_DEFAULT); //Place Player helper
        }
    }
    TCOD_console_set_default_foreground(NULL,TCOD_red);
    if(TCOD_console_get_char(NULL,PLYpos,CON_Y-1)!=TCOD_CHAR_BLOCK3) TCOD_console_put_char(NULL,PLYpos,CON_Y-1,'@',TCOD_BKGND_DEFAULT); //Place Player
    else CRASHED=PLYCRASHED; //Player Crashed!
    TCOD_console_set_default_foreground(NULL,TCOD_white);
    if(CRASHED!=NONE) CrashAnimation();
    else PrintIndicators();
    TCOD_console_flush(); //Apply changes to console
}
/** This function resets everything */
void Reset() {
    int i;
    TCOD_console_print(NULL,0,0,"Regenerating please wait...");
    TCOD_console_flush();
    InitRandomLevel();
    if(LearnedAISize!=0) {
        for(i=0; i<LearnedAISize; i++) free(LearnedAI[i]);
    }
    if(LearnedAI!=NULL) free(LearnedAI);
    LearnedAISize=0;
    TeachAI(); //Teach AI
}

int main() {
    srand(time(NULL)); //seed of the random
    TCOD_console_init_root(BLOCK_WDTH*2+1,CON_Y,"Learning AI",0,TCOD_RENDERER_SDL); //Console width: Block+Separator+Block
    TCOD_sys_set_fps(FPS);
    TCOD_console_set_keyboard_repeat(1,200);
    InitRandomLevel(); //Initialize random level
    TeachAI(); //Teach AI
    Timer=TCOD_sys_elapsed_milli(); //Timing
    do {
        PrintLevels();
        if(key.vk!=TCODK_ESCAPE) Wait(WAIT);
        Timer=TCOD_sys_elapsed_milli(); //Timing
        Move();
        ScrollLevels();
        if(CRASHED!=NONE) Reset();
    } while(key.vk!=TCODK_ESCAPE&&!TCOD_console_is_window_closed());

    return 0;
}
