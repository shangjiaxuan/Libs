#include <iostream>
#include <random>
#include <cassert>
#include <stdio.h>

constexpr double PI = 3.1415926535;
constexpr double V0 = 3*PI*PI/2.0;

constexpr int  l0 = 10000;
constexpr int dim = 2*l0+1;
constexpr int sects = 150;
constexpr int bin_num = sects*2 + 1;

void prepare_matrix(double a[dim], double b[dim], double k)
{
	for (int64_t i = 0; i < dim; ++i) {
		double temp = k+(2*PI*(i-l0));
		a[i]=(temp*temp/2.0)-(V0/2.0);
	}
	b[0]= 0;
	for (int i = 0; i < dim; ++i) {
		b[i]=-V0/4.0;
	}
}

double estimate_max(double a[dim], double b[dim], double x[dim], double temp[dim])
{
	double last_time=0;
	while(true){
		for (int i = 0; i < dim; ++i) {
			temp[i] = a[i] * x[i];
			if (i != 0)
				temp[i] += b[i - 1] * x[i - 1];
			if (i != dim-1)
				temp[i] += b[i + 1] * x[i + 1];
		}
		double max = 0;
		for (int i = 0; i < dim; ++i) {
			if (abs(temp[i]) > max) max = temp[i];
		}
		for (int i = 0; i < dim; ++i) {
			x[i] = temp[i] / max;
		}
		if (abs(abs(last_time) - abs(max))/abs(max)<0.000001) {
			last_time = max;
			break;
		}
		last_time = max;
	}
	return abs(last_time);
}

int num_roots(double a[dim], double b[dim], double mu)
{
	int num = 0;
	double q = a[0] - mu;
	for (int i = 0; i < dim; ++i) {
		if(q<0) ++num;
		if (i != dim-1) {
			if (q == 0) {
				q = 0.00000000000001*abs(b[i+1]);
			}
			q=a[i+1]-mu-((b[i+1]*b[i+1])/q);
		}
	}
	return num;
}

//huge stack for user mode function
//but the stack has few layers of calls
//thus not a problem
double solve_matrix(double k)
{
	double a[dim]{};
	double b[dim]{};
	double x[dim]{};
	prepare_matrix(a,b,k);
	//in some bad cases, this does not give all the vectors
	//and the max eigen is not reached.
	double max = 0;
	std::random_device dev;
	uint64_t seed = uint64_t(dev())<<32| dev();
	std::mt19937_64 ran(seed);
	std::uniform_real_distribution<> dist(-1.0,1.0);
	int num = 0;
	do {
		//sometimes the max cannot easily found with random
		//startng vector, forcedly expanding the area can
		//speed up finding this.
		max*=2;
		for (int i = 0; i < dim; ++i) {
			x[i] = dist(ran);
		}
		double temp[dim]{};
		double cur = estimate_max(a, b, x, temp);
		max = (cur > max) ? cur : max;
		++num;
	}while(num_roots(a, b, max)<dim);
	double min = -max;
	bool found = false;
	int last_shorten = 0;
	double prev_max = max; 
	while (((max-min)/abs(max))>0.0000000000001) {
		//change the number here to plot other bands
		while (num_roots(a, b, max) > 0) {
			prev_max = max;
			max = (max+min)/2.0;
		}
		min = max;
		max = prev_max;
	}
//	assert(num_roots(a,b, max+abs(max*0.01))==1);
//	assert(num_roots(a,b, max-abs(max*0.01))==0);
	return max;
}

#include <TGraph.h>


int main()
{
	double vals[bin_num];
	double x[bin_num];
	for (int i = -sects; i <= sects; ++i) {
		x[i + sects] = double(i)/sects;
		vals[i+sects] = solve_matrix(double(i)/sects);
		printf("%.14f\n",vals[i+sects]);
	}
	TGraph graph(bin_num,x,vals);
	graph.DrawClone("AC*");
	return 0;
}
