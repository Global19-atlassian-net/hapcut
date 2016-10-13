
// function calculates both MEC likelihood and chimeric fragment likelihood 
void calculate_fragscore(struct fragment* Flist,int f, char* h,float* mec_ll,float* chimeric_ll)
{
	int j=0,k=0; float p0=0,p1=0,prob =0,prob1=0,prob2=0; float chim_prob = -1000000;
	int bit = 0,bits=0;

	for (j=0;j<Flist[f].blocks;j++)
	{
		for (k=0;k<Flist[f].list[j].len;k++)
		{
			if (h[Flist[f].list[j].offset+k] == '-' || (int)Flist[f].list[j].qv[k] -QVoffset < MINQ) continue; 
			prob = QVoffset-(int)Flist[f].list[j].qv[k]; prob /= 10; prob1 = 1.0; prob1 -= pow(10,prob); prob2 = log10(prob1);
			if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) { p0 += prob2; p1 +=  prob; } else { p0 += prob; p1 += prob2; }
			bit +=1;  // counter over the alleles of the fragment, ignore invalid alleles 
		}
	}
	bits = bit; bit = 0;

	if (bits > 2) 
	{
		for (j=0;j<Flist[f].blocks;j++)
		{
			for (k=0;k<Flist[f].list[j].len;k++)
			{
				if (h[Flist[f].list[j].offset+k] == '-' || (int)Flist[f].list[j].qv[k] -QVoffset < MINQ) continue; 
				prob = QVoffset-(int)Flist[f].list[j].qv[k]; prob /= 10; prob1 = 1.0; prob1 -= pow(10,prob); prob2 = log10(prob1);
				
				if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) 
				{ 
					p0 -= prob2; p0 += prob; 
					p1 -=  prob; p1 += prob2; 
				}
				else 
				{ 
					p0 -= prob; p0 += prob2;
					p1 -= prob2; p1 += prob;
				}
				if (bit > 0 && bit < bits-1) // add the switch error likelihood to chim_prob
				{
					if (p0 > chim_prob) chim_prob = p0 + log10(1.0+pow(10,chim_prob-p0)); else chim_prob += log10(1.0+pow(10,p0-chim_prob)); 
					if (p1 > chim_prob) chim_prob = p1 + log10(1.0+pow(10,chim_prob-p1)); else chim_prob += log10(1.0+pow(10,p1-chim_prob)); 
				}
				bit +=1;  
			}
		}
		chim_prob -= log10(bits-2); 
	}
	*chimeric_ll = chim_prob; 

	if (p0 > p1) *mec_ll =  (p0 + log10(1+pow(10,p1-p0))); else *mec_ll =  (p1 + log10(1+pow(10,p0-p1)));
	//return mec_prob; 
}

// function to compute weight of a edge between two variants in the max-cut graph (score of +1 if the phasing agrees with the phasing suggested by the fragment and score of -1 if it does not)
// this function needs to be modified to utilize base-quality scores, so that the weight of an edge is (1-e1)(1-e2) for the two base-calls
float edge_weight(char* hap,int i, int j, char* p,struct fragment* Flist,int f) 
{
	float q1=1,q2=1;		int k=0,l=0;
	// new code added so that edges are weighted by quality scores, running time is linear in length of fragment !! 08/15/13 | reduce this
	for (k=0;k<Flist[f].blocks;k++)
	{
		for (l=0;l<Flist[f].list[k].len;l++) 
		{
			if (Flist[f].list[k].offset + l == i) q1 = Flist[f].list[k].pv[l]; 
			else if (Flist[f].list[k].offset + l == j) q2 = Flist[f].list[k].pv[l]; 
		}
	}
	float p1 = q1*q2+(1-q1)*(1-q2); float p2 = q1*(1-q2)+q2*(1-q1);
	if (hap[i] == hap[j] && p[0] == p[1]) return log10(p1/p2);
	else if (hap[i] != hap[j] && p[0] != p[1]) return log10(p1/p2);
	else if (hap[i] == hap[j] && p[0] != p[1]) return log10(p2/p1);
	else if (hap[i] != hap[j] && p[0] == p[1]) return log10(p2/p1); 
	else return 0;
}


// compute MECSCORE of the fragment matrix compared to a haplotype 'h' 
int mecscore(struct fragment* Flist,int fragments, char* h,float* ll, float* calls,float* miscalls)
{
	//	fprintf(stderr,"QVoffset is now %d \n",QVoffset);
	int j=0,k=0,f=0; *ll =0; float p0,p1; *calls =0; *miscalls=0; float prob; float prob1; float prob2;
	float good=0,bad=0; int switches =0; int m=0; 
	for (f=0;f<fragments;f++)	
	{
		good=bad=0;p0=p1=0; //if (Flist[f].blocks ==1 && Flist[f].list[0].len ==1) continue;
		if (h[Flist[f].list[0].offset] == Flist[f].list[0].hap[0]) m = 1; else m = -1;  // initialize 
		switches = 0;

		for (j=0;j<Flist[f].blocks;j++)
		{
			*calls += Flist[f].list[j].len;
			for (k=0;k<Flist[f].list[j].len;k++)
			{
				if (h[Flist[f].list[j].offset+k] == '-' ) continue;
				if ((int)Flist[f].list[j].qv[k] -QVoffset < MINQ) continue; 
				prob = QVoffset-(int)Flist[f].list[j].qv[k]; prob /= 10; prob1 = 1.0 - pow(10,prob); prob2 = log10(prob1); // changed 03/03/15 
				
				//if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) good +=1; else bad +=1;
				if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) good +=prob1; else bad +=prob1;
				if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k] && m == -1)
				{
					m = 1; switches++;
				}
				else if (h[Flist[f].list[j].offset+k] != Flist[f].list[j].hap[k] && m == 1)
				{
					m = -1; switches++;
				}
				prob1 = log10(prob1);
				//printf("prob %s %f %f \n",Flist[f].list[j].qv,prob,prob1); 
				if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) { p0 += prob2; p1 +=  prob; }
				else { p0 += prob; p1 += prob2; }
			}
		}
		if (SCORING_FUNCTION==0) 
		{
			if (good < bad) *miscalls += good; else *miscalls += bad;
		}
		else if (SCORING_FUNCTION ==1) *miscalls += switches; 
		else 
		{
			if (switches ==2 && (good <= 1.0 || bad <= 1.0) ) *miscalls += 1; 
			else if (switches ==4 && (good <= 2.0 || bad <= 2.0) ) *miscalls += 2; 
			else *miscalls += switches; 
		}
		//fprintf(stdout,"good %f bad %f frag %d %f\n",good,bad,f,*calls);
		if (p0 > p1) *ll += (p0 + log10(1+pow(10,p1-p0))); else *ll += (p1 + log10(1+pow(10,p0-p1))); 
	}
	return *calls;
}

void update_fragscore(struct fragment* Flist,int f, char* h)
{
	int j=0,k=0; float p0=0,p1=0,prob =0,prob1=0,prob2=0;
	Flist[f].calls =0;
	float good=0,bad=0; int switches =0; int m=0; 
	if (h[Flist[f].list[0].offset] == Flist[f].list[0].hap[0]) m = 1; else m = -1;  // initialize 
	for (j=0;j<Flist[f].blocks;j++) 
	{
		Flist[f].calls += Flist[f].list[j].len;
		for (k=0;k<Flist[f].list[j].len;k++) 
		{
			if (h[Flist[f].list[j].offset+k] == '-') continue;// { fprintf(stdout,"fragment error"); continue;}
			if ((int)Flist[f].list[j].qv[k] -QVoffset < MINQ) continue; 
			prob = QVoffset-(int)Flist[f].list[j].qv[k]; prob /= 10; prob1 = 1.0; prob1 -= pow(10,prob); prob2 = log10(prob1);
			if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) good +=prob1; else bad +=prob1; 
			//if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) good++; else bad++;
			if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) { p0 += prob2; p1 +=  prob; }
			else { p0 += prob; p1 += prob2; }
			if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k] && m == -1)
			{
				m = 1; switches++;
			}
			else if (h[Flist[f].list[j].offset+k] != Flist[f].list[j].hap[k] && m == 1)
			{
				m = -1; switches++;
			}
		}
	}
	if (p0 > p1) Flist[f].ll = (p0 + log10(1+pow(10,p1-p0))); else Flist[f].ll = (p1 + log10(1+pow(10,p0-p1)));

	if (SCORING_FUNCTION ==0) 
	{
		if (good < bad) Flist[f].currscore = good; else Flist[f].currscore = bad;
	}
	else if (SCORING_FUNCTION ==1) Flist[f].currscore = switches; 
	else
	{
		if (switches ==2 && (good <= 1.0 || bad <= 1.0) ) Flist[f].currscore += 1; 
		else if (switches ==4 && (good <= 2.0 || bad <= 2.0) ) Flist[f].currscore += 2; 
		else Flist[f].currscore = switches; 
	}
}

// change this to function that calculates posterior probability of fragment being HAP1, HAP2, HAP1:HAP2, HAP2:HAP1 etc ....

// function to print comparison of fragment to haplotype | format is 45333 000:010:BAF  (fragment block):(hap block):(qscore block)
int print_fragment_MEC(struct fragment* Flist,int f,char* h,FILE* outfile)
{
	int j=0,k=0; float prob =0,prob1=0, good=0,bad=0,mec=0;
	int hap =0;
	int switches =0; int m=0; int mm=0;
	if (h[Flist[f].list[0].offset] == Flist[f].list[0].hap[0]) m = 1; else m = -1;  // initialize 

	for (j=0;j<Flist[f].blocks;j++) 
	{
		for (k=0;k<Flist[f].list[j].len;k++) 
		{
			if (h[Flist[f].list[j].offset+k] == '-') continue;// { fprintf(stdout,"fragment error"); continue;}
			if ((int)Flist[f].list[j].qv[k] -QVoffset < MINQ) continue; 
			prob = QVoffset-(int)Flist[f].list[j].qv[k]; prob /= 10; prob1 = 1.0; prob1 -= pow(10,prob);
			if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) good +=prob1; else bad +=prob1; 
			if (h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k] && m == -1)
			{
				m = 1; switches++;
			}
			else if (h[Flist[f].list[j].offset+k] != Flist[f].list[j].hap[k] && m == 1)
			{
				m = -1; switches++;
			}
		}
	}
	if (good < bad) { mec = good; hap = 0; } else { mec = bad; hap = 1; } 

	float mec_ll,chimeric_ll; calculate_fragscore(Flist,f,h,&mec_ll,&chimeric_ll);
	//if (mec <=0.001) return 0;
	if (chimeric_ll >= mec_ll+3) fprintf(outfile,"SER "); else fprintf(outfile,"MEC ");
	//if (mec > 1.99 && switches ==1) fprintf(outfile,"SER "); else fprintf(outfile,"MEC ");
	fprintf(outfile,"%0.2f %0.2f ",mec_ll,chimeric_ll); 
	fprintf(outfile,"%0.1f %d %d %s ",mec,switches,Flist[f].blocks,Flist[f].id); 
	fprintf(outfile,"%d ",Flist[f].list[0].offset); 


	for (j=0;j<Flist[f].blocks;j++) 
	{
		mm =0;
		//fprintf(outfile,"%d ",Flist[f].list[j].offset); 
		for (k=0;k<Flist[f].list[j].len;k++) fprintf(outfile,"%c",Flist[f].list[j].hap[k]); 
		fprintf(outfile,":"); 
		for (k=0;k<Flist[f].list[j].len;k++) 
		{
			if (hap ==1 && h[Flist[f].list[j].offset+k] != Flist[f].list[j].hap[k]) mm++; 
			else if (hap ==0 && h[Flist[f].list[j].offset+k] == Flist[f].list[j].hap[k]) mm++; 

			if (hap ==1) fprintf(outfile,"%c",h[Flist[f].list[j].offset+k]); 
			else if (h[Flist[f].list[j].offset+k] == '0') fprintf(outfile,"1");
			else if (h[Flist[f].list[j].offset+k] == '1') fprintf(outfile,"0");
			else fprintf(outfile,"-"); 
		}
		if (mm > 0) { fprintf(outfile,":"); for (k=0;k<Flist[f].list[j].len;k++) fprintf(outfile,"%d,",(int)Flist[f].list[j].qv[k]-33); } 
		fprintf(outfile," ");
	}
	fprintf(outfile,"\n");
}

void print_fragmentmatrix_MEC(struct fragment* Flist,int fragments,char* h,char* outfileprefix)
{
	fprintf(stderr,"printing fragment matrix along with MEC scores to a file \n");
	char outfile[1024]; sprintf(outfile,"%s.fragments",outfileprefix);
	FILE* fp = fopen(outfile,"w");
	int i=0;
	for (i=0;i<fragments;i++) print_fragment_MEC(Flist,i,h,fp); 
	fclose(fp);
}


