#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#include "Utils.h"
#include "MyR.h"
#include "MyRandom.h"
#include "Tree.h"
#include "Fossil.h"
#include "FossilInt.h"
#include "PiecewiseModel.h"
#include "FBDDensity.h"
#include "SimulPiecewise.h"
#include "MCMCImportanceSamplingFossilInt.h"
#include "MinimizeNLOpt.h"

//./xfbd -u 40 -p -20. 0.5 0.1 0.5 1. -p -10 0.05 0.2 0.1 1. -s 5000 -b 10000 -g 50 -f 1. -a 0.33 0.33 -w 0.5 0.5 0.5 -i 5. 5. 5.  -r 0000 
//valgrind ./dfbd -s 200 ~/Dropbox/FossilPiecewise/dataMS/TreeEupely2.phy ~/Dropbox/FossilPiecewise/dataMS/CotylosauriaAgesNew.csv ~/Dropbox/FossilPiecewise/modelMS/schemeM0.txt

#define NAME_CODA "mcmc_sample"

#define MAX_PARAM 10

#define STRING_SIZE 300
#define HELP_MESSAGE "\nusage: sample [options] [<output file>]\n\nsample simulates random trees and fossils finds and saves them in Newick format\n\nOptions:\n\t-h : display help\n\t-b <birth>\t: set birth rate\n\t-d <death>\t: set death rate\n\t-f <fossil>\t: set fossil find rate\n\t-m <min>\t: set minimum number of contemporary species of a simulation to be considered\n\t-M <size>\t: set maximum size of a simulation to be considered\n\t-i <niter>\t: set the number of simulations\n\t-t <time> : the end time of the diversification (start is always 0)\n"


//./dfbd -s 20000 ../../data/TreeEupely40.phy ../../data/CotylosauriaAgesNew.csv model/schemeExtKR.txt 


void nameLeavesRec(int n, int *ind, int length, TypeTree *tree);
void renameLeaves(TypeTree *tree);
TypeTree *getRandomTreeFossil(TypePiecewiseModelParam *param, int minSizeTree, int maxSizeTree, int minSlice, double stopTime, void *rand_data);
TypeTree *getRandomTreeFossilInt(TypePiecewiseModelParam *param, int minSizeTree, int maxSizeTree, int minSlice, double stopTime, double fos_width, void *rand_data);


int main(int argc, char **argv) {	
	char *inputFileNameTree, *inputFileNameFossil, *inputFileNameScheme, outputName[STRING_SIZE], outputFileName[STRING_SIZE], *outputPrefix, option[256];
	FILE *fit, *fif, *fis, *fo;
	int i, niter = 5, nSamp = 5000, nBurn = 10000, nGap = 10;
	double al = 0.75, probSpe = 0.25, probExt = 0.25, probFos = 0.25, propParam = 0.2, stopTime = DBL_MAX;
	TypeModelParam windSize = {.birth=0.05, .death = 0.05, .fossil = 0.05, .sampling = 0.5}, init = {.birth=0.5, .death = 0.5, .fossil = 0.1, .sampling = 1.};
	TypeNLOptOption nloptOption;

	nloptOption.infSpe = 0.;
	nloptOption.supSpe = 1.;
	nloptOption.infExt = 0.;
	nloptOption.supExt = 1.;
	nloptOption.infFos = 0.;
	nloptOption.supFos = 1.;
	nloptOption.trials = 10;
	nloptOption.tolOptim = 0.001;
	nloptOption.maxIter = 1000;

	for(i=0; i<256; i++)
		option[i] = 0;
	for(i=1; i<argc && *(argv[i]) == '-'; i++) {
		int j;
		for(j=1; argv[i][j] != '\0'; j++)
			option[argv[i][j]] = 1;
		if(option['n']) {
			option['n'] = 0;
			if((i+1)<argc) {
				FILE *fopt;
				if((fopt = fopen(argv[++i], "r"))) {
					fscanNLoptOptionTag(fopt, &nloptOption);
					fclose(fopt);
				} else {
					fprintf(stderr, "Can't open file %s\n", argv[++i]);
					exit(EXIT_FAILURE);
				}
			} else 
				error("File name missing after option -o\n");
		}
		if(option['a']) {
			option['a'] = 0;
			if((i+1)<argc && sscanf(argv[i+1], "%le", &probSpe) == 1)
				i++;
			else
				error("3 values are expected after -a");
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &probExt) == 1)
				i++;
			else
				error("3 values are expected after -a");
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &probFos) == 1)
				i++;
			else
				error("3 values are expected after -a");
		}
		if(option['w']) {
			option['w'] = 0;
			if((i+1)<argc && sscanf(argv[i+1], "%le", &windSize.birth) == 1)
				i++;
			else
				error("4 values are expected after -w");
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &windSize.death) == 1)
				i++;
			else
				error("4 values are expected after -w");
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &windSize.fossil) == 1)
				i++;
			else
				error("4 values are expected after -w");
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &windSize.sampling) == 1)
				i++;
			else
				error("4 values are expected after -w");
		}
		if(option['i']) {
			option['i'] = 0;
			if((i+1)<argc && sscanf(argv[i+1], "%le", &init.birth) == 1)
				i++;
			else
				error("4 values are expected after -w");
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &init.death) == 1)
				i++;
			else
				error("4 values are expected after -w");
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &init.fossil) == 1)
				i++;
			else
				error("4 values are expected after -w");
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &init.sampling) == 1)
				i++;
			else
				error("4 values are expected after -w");
		}
		if(option['s']) {
			option['s'] = 0;
			if((i+1)<argc && sscanf(argv[i+1], "%d", &nSamp) == 1)
				i++;
			else
				error("a number is expected after -s");
		}
		if(option['b']) {
			option['b'] = 0;
			if((i+1)<argc && sscanf(argv[i+1], "%d", &nBurn) == 1)
				i++;
			else
				error("a number is expected after -b");
		}
		if(option['g']) {
			option['g'] = 0;
			if((i+1)<argc && sscanf(argv[i+1], "%d", &nGap) == 1)
				i++;
			else
				error("a number is expected after -b");
		}
		if(option['u']) {
			option['u'] = 0;
			if((i+1)<argc && sscanf(argv[i+1], "%d", &niter) == 1)
				i++;
		}
		if(option['S']) {
			option['S'] = 0;
			if((i+1)<argc && sscanf(argv[i+1], "%lf", &stopTime) == 1)
				i++;
		}
		if(option['h']) {
			printf("%s\n", HELP_MESSAGE);
			return 0;
		}
	}
	if(i<argc) {
		inputFileNameTree = argv[i++];
	} else {
		fprintf(stderr, "Please provide the name of a file containing a phylogenetic tree in Newick format\n");
		exit(1);
	}
	if(i<argc) {
		inputFileNameFossil = argv[i++];
	} else {
		fprintf(stderr, "Please provide the name of a file containing a fossil list\n");
		exit(1);
	}
	if(i<argc) {
		inputFileNameScheme = argv[i++];
	} else {
		fprintf(stderr, "Please provide the name of a file containing a  model scheme\n");
		exit(1);
	}
	strcpy(outputName, inputFileNameScheme);
	if((outputPrefix = strrchr(outputName, '.')) != NULL)
		outputPrefix[0] = '\0';
	if((outputPrefix=strrchr(outputName, '/')) == NULL)
		outputPrefix = outputName;
	else
		outputPrefix++;
	
	if((fit = fopen(inputFileNameTree, "r")) && (fif = fopen(inputFileNameFossil, "r")) && (fis = fopen(inputFileNameScheme, "r"))) {
		TypeTree **tree;
		TypeFossilIntFeature *fint;
		TypeSamplingScheme scheme;
		TypePiecewiseModelParam maxParam;
		int sizeTree, nParam, n;
		double logLike;
		void *rand_data;
		rand_data = my_rand_get_data();
		scheme = readSamplingScheme(fis);
        fclose(fis);
		tree = readTrees(fit);
        fclose(fit);
		sizeTree = 0;
		for(i=0; tree[i]!=NULL; i++) {
			int n;
			toBinary(tree[i]);
			if(tree[i]->name!=NULL)
				for(n=0; n<tree[i]->size; n++)
					if(tree[i]->name[n]!=NULL)
						fixSpace(tree[i]->name[n]);
			tree[i]->minTime = scheme.param.startTime[0];
			tree[i]->maxTime = scheme.param.startTime[scheme.param.size];
			sizeTree++;
		}
		if(sizeTree == 0)
			error("No tree!\n");
		reindexTreesFromName(tree, sizeTree);

		//for(n=0; n<tree->size; n++)
			//if(tree->time[n] < scheme.param.startTime[scheme.param.size])
				//tree->time[n] = NO_TIME;
			//else
				//tree->time[n] = scheme.param.startTime[scheme.param.size];
		fint = getFossilIntFeature(fif, tree[0]->name, tree[0]->size);
        fclose(fif);
		if(getMaxFossilIntTime(fint) > 0.)
			negateFossilInt(fint);
		fixStatus(tree[0], fint);
		for(n=0; n<tree[0]->size; n++)
			if(fint->status[n] == unknownNodeStatus)
				for(i=0; tree[i]!=NULL; i++)
					tree[i]->time[n] = fint->endTimeTable[n].inf;
		nParam = scheme.sizeParamBirth+scheme.sizeParamDeath+scheme.sizeParamFossil+scheme.sizeParamSampling;
		maxParam.param = (TypeModelParam*) malloc(scheme.param.size*sizeof(TypeModelParam));
		maxParam.startTime = scheme.param.startTime;
		//logLike = MCMCFillMaxParamSampleSC(tree, sizeTree, fint, scheme, al, nSamp, propParam, &windSize, &init, probSpe,probExt, probFos, &maxParam, rand_data);
		//printf("log-Likelihood\t%le\nAIC\t%le\n", logLike, 2.*(((double)nParam)-logLike));
		//sprintf(outputFileName, "%s_ML_model.txt", outputPrefix);
		//if((fo = fopen(outputFileName, "w"))) {
			//printPiecewiseModel(fo, &maxParam);
			//fclose(fo);
		//}
		logLike = MCMCGetMaxLogLikelihoodSCNLOpt(tree, sizeTree, fint, scheme, &nloptOption, al, nSamp, propParam, &windSize, &init, probSpe,probExt, probFos, rand_data);
		freeSamplingScheme(&scheme);
		free((void*)maxParam.param);
//(TypeTree **tree, int nTree, TypeFossilIntFeature *fi, TypeSamplingScheme sampScheme, TypeNLOptOption *option, double al, int iter, double prop, TypeModelParam *windSize, TypeModelParam *init, double probSpe, double probExt, double probFos, void *rand_data);
	}
	return 0;
}

void nameLeavesRec(int n, int *ind, int length, TypeTree *tree) {
	if(tree->node[n].child < 0) {
		int i, tmp;
		tree->name[n] = (char*) malloc((length+1)*sizeof(char));
		tmp = *ind;
		tree->name[n][length] = '\0';
		for(i=0; i<length; i++) {
			tree->name[n][length-1-i] = '0'+tmp%10;
			tmp /= 10;
		}
		(*ind)++;
	}
}

void renameLeaves(TypeTree *tree) {
	int length, ind;
	if(tree->name == NULL) {
		int i;
		tree->name = (char**) malloc(tree->sizeBuf*sizeof(char*));
		for(i=0; i<tree->sizeBuf; i++)
			tree->name[i] = NULL;
	}
	length = (int) ceil(log(tree->size)/log(10.));
	ind = 0;
	nameLeavesRec(tree->root, &ind, length, tree);
}

TypeTree *getRandomTreeFossil(TypePiecewiseModelParam *param, int minSizeTree, int maxSizeTree, int minSliceB, double stopTime, void *rand_data) {
	TypeTree *tree, *treeTmp;
	int n, *count, minSlice;
	tree = NULL;
	count = (int*) malloc(param->size*sizeof(int));
	do {
		if(tree != NULL) {
			if(tree->info != NULL) {
				freeFossilFeature((TypeFossilFeature*) tree->info);
				tree->info = NULL;	
			}
			freeTree(tree);
			tree = NULL;
		}
		treeTmp = simulTreePiecewise(param, rand_data);
		if(treeTmp != NULL && treeTmp->size > minSizeTree) {
			tree = pruneFossilBis(treeTmp, (TypeFossilFeature*)treeTmp->info);
			freeFossilFeature((TypeFossilFeature*)treeTmp->info);
			treeTmp->info = NULL;
			freeTree(treeTmp);
			treeTmp = tree;
			tree = fixBinaryFossil(treeTmp, (TypeFossilFeature*) treeTmp->info);
			freeFossilFeature((TypeFossilFeature*)treeTmp->info);
			treeTmp->info = NULL;
			freeTree(treeTmp);
		} else {
			if(treeTmp != NULL) {
				freeFossilFeature((TypeFossilFeature*)treeTmp->info);
				treeTmp->info = NULL;
				freeTree(treeTmp);
			}
			tree = NULL;
		}
		if(tree != NULL) {
			int i;
			countNodeTime(count, tree, param->size, param->startTime);
			minSlice = count[0];
			for(i=1; i<param->size; i++)
				if(count[i] < minSlice)
					minSlice = count[i];
		} else
			minSlice = 0;
	} while(!(tree != NULL && tree->size >= minSizeTree  && minSlice >= minSliceB && tree->size <= maxSizeTree));
//	} while(!(tree != NULL && minSlice>minSizeTree && tree->size<maxSizeTree));
	free((void*)count);
	treeTmp = tree;
	if(stopTime != DBL_MAX) {
		tree = stopTreeFossil(treeTmp, (TypeFossilFeature*) treeTmp->info, stopTime);
		freeFossilFeature((TypeFossilFeature*)treeTmp->info);
		treeTmp->info = NULL;
		freeTree(treeTmp);
	}
	tree->minTimeInt.inf = tree->minTime;
	tree->minTimeInt.sup = tree->minTime;
	for(n=0; n <tree->size; n++)
		if(tree->node[n].child != NOSUCH)
			tree->time[n] = NO_TIME;
	return tree;
}

TypeTree *getRandomTreeFossilInt(TypePiecewiseModelParam *param, int minSizeTree, int maxSizeTree, int minSlice, double stopTime, double fos_width, void *rand_data) {
	TypeTree *tree;
	TypeFossilIntFeature *fint;
	TypeSampleIntData siData;
	tree = getRandomTreeFossil(param, minSizeTree, maxSizeTree, minSlice, stopTime, rand_data);
	siData.min = param->startTime[0];
	siData.max = (stopTime!=DBL_MAX)?stopTime:param->startTime[param->size];
	siData.length = fos_width;
	fint = sampleFossilIntFromFossil((TypeFossilFeature*) tree->info, tree->size, &siData, sampleFossilIntFixed, rand_data);
	freeFossilFeature((TypeFossilFeature*) tree->info);
	tree->info = fint;
	return tree;
}