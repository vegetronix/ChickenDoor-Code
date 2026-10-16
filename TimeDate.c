// TimeDate.c

// modified 20090324 - 	added DateTimeOldest, DateTimeSet, and DateTimeGet
// 						corrected some typos, as indicated below

/*************************************************
Copyright 2009 Daycounter, Inc. 
Contact Info:
Daycounter, Inc. -  Contract Electronics and Software Engineering.
www.daycounter.com
Phone: 801-523-8444
*************************************************/

#include "TimeDate.h"

TimeDate gTime;
BOOL IsLeapYear(int year);

// dates when the day is the doomsday (these days are all the same day of the week each year)
const int DoomsDays[12]={3,28,7,4,9,6,11,8,5,10,7,12}; // one day listed for each month
// Sunday = 0, Monday = 1, ..., Saturday = 6
// http://en.wikipedia.org/wiki/Doomsday_algorithm
int DayOfTheWeek(TimeDate *t){
	int offset,dotw,dotw_r;
	int y = t->year%100;
	int ThisDoomsday = (ANCHOR_DAY + y + (y/4) - (y/100) + (y/400))%7; // day of the week the doomsday is on for this year
	int base = DoomsDays[t->month];										 // date in the current month for the doomsday
	if (IsLeapYear(t->year)){
		if (t->month <= 1) base++;
	}
	offset = t->day - base;
	dotw = ThisDoomsday + offset;
	while(dotw<0) dotw += 7;
	dotw_r = dotw%7;
	return dotw_r;
}

/* // this lookup table method might be faster than the mod method below
const WORD LeapYears[22]={
	2012,2016,2020,2024,2028,2032,2036,2040,2044,2048,2052,2056,2060,2064,
	2068,2072,2076,2080,2084,2088,2092,2096};
BOOL IsLeapYear(year){
	int ii;
	for (ii=0;ii<22;ii++){
		if (year == LeapYears[ii]){
			return true;
		}
	}
	return false;
}
*/

BOOL IsLeapYear(int year) {
	return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

int DaysPerMonth(int month, int year) {
	switch(month) {
		case 1:		// feb
			if(IsLeapYear(year)) return 29;
			else return 28;
		case 0:		// jan
		case 2:		// mar
		case 4:		// may
		case 6:		// jul
		case 7:		// Aug
		case 9:		// oct
		case 11:	// dec
			return 31;		
		case 3:		// apr 30
		case 5:		// jun 30
		case 10:	// nov 30						
		case 8:		// sep 30
			return 30;	
	}
	return 0;
}



void TimeDateProc(void) { // place in timer ISR
	gTime.tic++;
	if(gTime.tic >= TICS_PER_SEC) {
		gTime.sec++;
		gTime.tic=0;
		if(gTime.sec >= 60) {
			gTime.min++;
			gTime.sec=0;
			if(gTime.min>=60) {
				gTime.hour++;
				gTime.min= 0;
				if(gTime.hour>=24) { 
					gTime.day++;
					gTime.hour= 0; 
					if(gTime.day > DaysPerMonth(gTime.month,gTime.year)) {
						gTime.day= 1;	
						gTime.month++;
						if(gTime.month >= 12) { // month is zero based, so (if == 12) must wrap.
							gTime.month= 0;
							gTime.year++;
						}
					}
				}
			}
		}
	}
}

void TimeDateGet(TimeDate *t){
	t->year = gTime.year;
	t->month = gTime.month;
	t->day = gTime.day;
	t->hour = gTime.hour;
	t->min = gTime.min;
	t->sec = gTime.sec;
	t->tic = gTime.tic;
}

void TimeDateSet(TimeDate* t){
	if ((t->year>=2009)&&(t->year<2100)){ // this check might not be necessary
		gTime.year = t->year;
	}
	if (t->month<=11){
		gTime.month = t->month;
	}
	if (t->day <= DaysPerMonth(gTime.month,gTime.year)){
		gTime.day = t->day;
	}
	if (t->hour<24){
		gTime.hour = t->hour; // military time
	}
	if (t->min<60){
		gTime.min = t->min;
	}
	if (t->sec<60){
		gTime.sec = t->sec;
	}
	gTime.tic = 0;
}

// returns true if t1 is older than t2, otherwise returns false
BOOL TimeDateOldest(TimeDate* t1,TimeDate* t2){
	if (t1->year < t2->year){
		return true;
	} else if (t1->year == t2->year){
		if (t1->month < t2->month){
			return true;
		} else if (t1->month == t2->month){
			if (t1->day < t2->day){
				return true;
			} else if (t1->day == t2->day){
				if (t1->hour < t2->hour){
					return true;
				} else if (t1->hour == t2->hour){
					if (t1->min < t2->min){
						return true;
					} else if (t1->min == t2->min){
						if (t1->sec < t2->sec){
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

void TimeDateCopy(TimeDate *dest,TimeDate *src) {
	dest->sec= src->sec;
	dest->min= src->min;
	dest->hour= src->hour;
	
	dest->day= src->day;
	dest->month= src->month;
	dest->year= src->year;
}


void TimeDateClear(TimeDate *t) {
	t->sec= 0;
	t->min= 0;
	t->hour= 0;
	
	t->day= 0;
	t->month= 0;
	t->year= 0;
}

int TimeDateCompare(TimeDate *a,TimeDate *b){ // 0 equal, -1: a<b, +1: a>b
	if(a->year < b->year) return -1;
	if(a->year > b->year) return +1;
	
	if(a->month < b->month) return -1;
	if(a->month > b->month) return +1;

	if(a->day < b->day) return -1;
	if(a->day > b->day) return +1;
	
	if(a->hour < b->hour) return -1;
	if(a->hour > b->hour) return +1;
	
	if(a->min < b->min) return -1;
	if(a->min > b->min) return +1;
	
	if(a->sec < b->sec) return -1;
	if(a->sec > b->sec) return +1;			
	return 0;	// equal. 
}

BOOL TimeDateInvalid(TimeDate *t) {
	if(t->year>=3000) return true;
	if(t->month>= 12) return true;
	if(t->day> DaysPerMonth(t->month,t->year)) return true; // days are 1 based.
	if(t->hour>= 24) return true;
	if(t->min>= 60) return true;
	if(t->sec>= 60) return true;
	return false;
}

BOOL TimeInvalid(BYTE sec,BYTE min,BYTE hour) {
	if(hour>= 24) return true;
	if(min>= 60) return true;
	if(sec>= 60) return true;
	return false;
}

BOOL DateInvalid(BYTE day, BYTE month,WORD year) {
	if(year >= 3000) return true;
	if(month >= 12) return true;
	if(day > DaysPerMonth(year,month)) return true; // days are 1 based.
	return false;
}

void TimeHourDec(TimeDate *td) {
	if(td->hour>0) td->hour--;
}
void TimeHourInc(TimeDate *td) {
	if(td->hour<23) td->hour++;
}
void TimeMinDec(TimeDate *td) {
	if(td->min>0) td->min--;
}
void TimeMinInc(TimeDate *td) {
	if(td->min<59) td->min++;
}

void TimeSecDec(TimeDate *td) {
	if(td->sec>0) td->sec--;
}
void TimeSecInc(TimeDate *td) {
	if(td->sec<59) td->sec++;
}

