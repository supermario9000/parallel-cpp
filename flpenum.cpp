#include <iostream>
#include <cmath>
#include <time.h>
#include <sys/time.h>

using namespace std;

//===== Globalus kintamieji ===================================================

int numDP = 5000;               // Vietoviu skaicius (demand points, max 10000)
int numPF = 5;                  // Esanciu objektu skaicius (preexisting facilities)
int numCL = 50;                 // Kandidatu naujiems objektams skaicius (candidate locations)
int numX  = 3;                  // Nauju objektu skaicius

double **demandPoints;          // Geografiniai duomenys
double **distanceMatrix;   	    // Masyvas atstumu matricai saugoti

int *X = new int[numX];         // Naujas sprendinys
int *bestX = new int[numX];     // Geriausias rastas sprendinys
double u, bestU;                // Naujo sprendinio ir geriausio sprendinio naudingumas (utility)

//===== Funkciju prototipai ===================================================

double getTime();
void loadDemandPoints();
double HaversineDistance(double lat1, double lon1, double lat2, double lon2);
double HaversineDistance(int i, int j);
double evaluateSolution(int* X);
int increaseX(int* X, int index, int maxindex);

//=============================================================================

int main() {
	loadDemandPoints();             // Duomenu nuskaitymas is failo	
    double t_start = getTime();     // Algoritmo vykdymo pradzios laikas

    //----- Atstumu matricos skaiciavimas -------------------------------------
    distanceMatrix = new double*[numDP];
	for (int i=0; i<numDP; i++) {
		distanceMatrix[i] = new double[i+1];
		for (int j=0; j<=i; j++) {
			distanceMatrix[i][j] = HaversineDistance(demandPoints[i][0], demandPoints[i][1], demandPoints[j][0], demandPoints[j][1]);
		}
	}
    double t_matrix = getTime();
    cout << "Matricos skaiciavimo trukme: " << t_matrix - t_start << endl;

    //----- Pradines naujo ir geriausio sprendiniu reiksmes -------------------
	for (int i=0; i<numX; i++) {    // Pradines naujo ir geriausio sprendiniu koordinates: [0,1,2,...]
		X[i] = i;
		bestX[i] = i;
	}
    u = evaluateSolution(X);        // Naujo sprendinio naudingumas (utility)
    bestU = u;                      // Geriausio sprendinio sprendinio naudingumas
		
    //----- Visų galimų sprendinių perrinkimas --------------------------------
	while (increaseX(X, numX-1, numCL) == true) {
        u = evaluateSolution(X);
        if (u > bestU) {
            bestU = u;
            for (int i=0; i<numX; i++) bestX[i] = X[i];
        }
	}
	
    //----- Rezultatu spausdinimas --------------------------------------------
	double t_finish = getTime();     // Skaiciavimu pabaigos laikas
	cout << "Sprendinio paieskos trukme: " << t_finish - t_matrix << endl;
    cout << "Algoritmo vykdymo trukme: " << t_finish - t_start << endl;
    cout << "Geriausias sprendinys: ";
	for (int i=0; i<numX; i++) cout << bestX[i] << " ";
	cout << "(" << bestU << " procentai rinkos)" << endl;
}

//===== Funkciju implementacijos (siu funkciju LYGIAGRETINTI NEREIKIA) ========

void loadDemandPoints() {
	FILE *f;
	f = fopen("demandPoints.dat", "r");
	demandPoints = new double*[numDP];
	for (int i=0; i<numDP; i++) {
		demandPoints[i] = new double[3];
		fscanf(f, "%lf%lf%lf", &demandPoints[i][0], &demandPoints[i][1], &demandPoints[i][2]);
	}
	fclose(f);
}

//=============================================================================

double HaversineDistance(double lat1, double lon1, double lat2, double lon2) {
	double dlat = fabs(lat1 - lat2);
	double dlon = fabs(lon1 - lon2);
	double aa = pow((sin((double)dlat/(double)2*0.01745)),2) + cos(lat1*0.01745) *
               cos(lat2*0.01745) * pow((sin((double)dlon/(double)2*0.01745)),2);
	double c = 2 * atan2(sqrt(aa), sqrt(1-aa));
	double d = 6371 * c; 
	return d;
}

double HaversineDistance(int i, int j) {
	if (i >= j)	return distanceMatrix[i][j];
	else return distanceMatrix[j][i];
}

//=============================================================================

double getTime() {
   struct timeval laikas;
   gettimeofday(&laikas, NULL);
   double rez = (double)laikas.tv_sec+(double)laikas.tv_usec/1000000;
   return rez;
}

//=============================================================================

double evaluateSolution(int *X) {
	double U = 0;
    double totalU = 0;
	int bestPF;
	int bestX;
	double d;
	for (int i=0; i<numDP; i++) {
        totalU += demandPoints[i][2];
		bestPF = 1e5;
		for (int j=0; j<numPF; j++) {
			d = HaversineDistance(i, j);
			if (d < bestPF) bestPF = d;
		}
		bestX = 1e5;
		for (int j=0; j<numX; j++) {
			d = HaversineDistance(i, X[j]);
			if (d < bestX) bestX = d;
		}
		if (bestX < bestPF) U += demandPoints[i][2];
		else if (bestX == bestPF) U += 0.3*demandPoints[i][2];
	}
	return U/totalU*100;
}

//=============================================================================

int increaseX(int *X, int index, int maxindex) {
	if (X[index]+1 < maxindex-(numX-index-1)) {
		X[index]++;
	}
	else {		 
		if ((index == 0) && (X[index]+1 == maxindex-(numX-index-1))) {
			return 0;
		}
		else {
			if (increaseX(X, index-1, maxindex)) X[index] = X[index-1]+1;
			else return 0;
		}	
	}
	return 1;
}