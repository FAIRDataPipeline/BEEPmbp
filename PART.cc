// This contains all the code related to particles in PMCMC

#include <iostream>
#include <algorithm>

using namespace std;

#include "math.h"

#include "timers.hh"
#include "utils.hh"
#include "PART.hh"
#include "output.hh"
#include "consts.hh"
#include "pack.hh"

PART::PART(DATA &data, MODEL &model, POPTREE &poptree) : data(data), model(model),  trans(model.trans), comp(model.comp), poptree(poptree), lev(poptree.lev)
{
}

/// Initialises a particle
void PART::partinit(unsigned int p)
{
	unsigned int c, cmax, j, jmax, dp, a;
	int l;
	double sum, val;

	fediv = data.fediv;
		
	pst = p;
	N.resize(comp.size()); for(c = 0; c < comp.size(); c++) N[c] = 0;
	N[0] = data.popsize;
 
	indinf.clear(); indinf.resize(data.narea);
	
	fev.clear(); fev.resize(fediv);

	Rtot.resize(poptree.level); sussum.resize(poptree.level);
	for(l = 0; l < int(poptree.level); l++){
		cmax = lev[l].node.size();
		Rtot[l].resize(cmax); sussum[l].resize(cmax);
		for(c = 0; c < cmax; c++) Rtot[l][c] = 0;
	}
	
	l = poptree.level-1;
	susage.resize(data.narea);
	Qmap.resize(data.narea);
	for(c = 0; c < data.narea; c++){ 
		susage[c].resize(data.nage); for(a = 0; a < data.nage; a++) susage[c][a] = 0;
		Qmap[c].resize(data.nage); for(a = 0; a < data.nage; a++) Qmap[c][a] = 0;
			
		sum = 0; 
		for(dp = 0; dp < data.ndemocatpos; dp++){
			val = model.sus[dp]*data.area[c].pop[dp];
			susage[c][data.democatpos[dp][0]] += val;
			sum += val;
		}
		sussum[l][c] = sum;
	}
			
	for(l = poptree.level-2; l >= 0; l--){                                                // Propages sussum up the tree
		cmax = poptree.lev[l].node.size();
		for(c = 0; c < cmax; c++){
			jmax =  poptree.lev[l].node[c].child.size(); 
			sum = 0; for(j = 0; j < jmax; j++) sum += sussum[l+1][poptree.lev[l].node[c].child[j]];
			sussum[l][c] = sum;
		}
	}
	 
	sussumst = sussum;
	susagest = susage;
	 
	sett = 0;
	
	tdnext = fediv;
	
	indev.resize(data.popsize);
}

/// Performs the modified Gillespie algorithm between times ti and tf 
void PART::gillespie(double ti, double tf, unsigned int outp)
{
	unsigned int c, j, jsel=0;
	double t, tpl;
	NEV n;
	vector <NEV> nev;
	
	if(sett == data.nsettime) emsg("Part: EC4");
						 
	t = ti; tpl = t;
	do{
		nev.clear();                     // First we decide what event is next
		n.t = data.settime[sett+1];
		n.type = SET_EV;
		nev.push_back(n); 
		 
		if(tdnext < fediv) n.t = fev[tdnext][tdfnext].t; else n.t = tf;
		n.type = FEV_EV;
		nev.push_back(n);
	
		if(Rtot[0][0] < tiny) n.t = tf; else n.t = t - log(ran())/(model.beta[sett]*Rtot[0][0]);
		n.type = INF_EV;
		nev.push_back(n);
		
		n.t = t - log(ran())/(sussum[0][0]*model.phi[sett]);

		n.type = EXT_EV;
		nev.push_back(n);
		
		if(outp == 1){
			while(t > tpl){ 
				cout  << "Time: " << tpl;
				for(c =0; c < comp.size(); c++) cout << "  " << comp[c].name << ":"	<< N[c];
				cout << endl;
				tpl++;
			}
		}

		t = tf; for(j = 0; j < nev.size(); j++){ if(nev[j].t < t){ t = nev[j].t; jsel = j;}}
		if(t == tf) break;

		switch(nev[jsel].type){
		case SET_EV:                 // These are "settime" events which allow the value of beta to change in time
			sett++; if(sett >= data.nsettime) emsg("Part: EC5");
			break;
		
		case INF_EV:                 // These are infection events within the system
 		case EXT_EV:                 // These are external infection event
	  	c = nextinfection(nev[0].type);
			addinfc(c,t);	
			break;
			
		case FEV_EV:                 // These correspond to other compartmental transitions (e.g. E->A, E->I etc...)
			dofe();
			break;
		
		default: emsg("Part: EC6"); break;
		}		
	}while(t < tf);

	if(checkon == 1) check(0,t);
}

/// Adds an exposed indivdual in area c
void PART::addinfc(unsigned int c, double t)
{
	unsigned int i, dp, j, jmax, k, kmax, a;
	int l;
	double dR, sum, sus, z;
	vector <double> sumst;
	vector <FEV> evlist;
	
	sum = 0; sumst.resize(data.ndemocatpos);
	for(dp = 0; dp < data.ndemocatpos; dp++){ 
		sum += data.area[c].pop[dp]*model.sus[dp];
		sumst[dp] = sum;
	}
	
	kmax = indinf[c].size();
	do{
	  z = ran()*sum;                                           // Samples in proportion to individual susceptibility
		dp = 0; while(dp < data.ndemocatpos && z > sumst[dp]) dp++; if(dp == data.ndemocatpos) emsg("Part: EC1");
		i = data.area[c].ind[dp][int(ran()*data.area[c].pop[dp])];
		
		for(k = 0; k < kmax; k++) if(indinf[c][k] == i) break;   // Checks selected individual is not infected
	}while(k < kmax);
	indinf[c].push_back(i);
	
	sus = model.sus[dp];
	
	a = data.democatpos[dp][0];
	susage[c][a] -= sus;
	dR = -sus*Qmap[c][a];
	
	l = poptree.level-1; 
	do{
		Rtot[l][c] += dR;
		sussum[l][c] -= sus;
		c = lev[l].node[c].parent; l--;
	}while(l >= 0);
	
	model.simmodel(indev[i],i,0,t);
	jmax = indev[i].size(); for(j = 0; j < jmax; j++) addfev(indev[i][j],data.period,t);
}

/// Used to check that various quantities are being correctly updated
void PART::check(unsigned int num, double t)
{
	unsigned int l, c, cmax, cc, k, kmax, j, dp, i, a, aa, v, q;
	double dd, sum, sum2, val, inf;
	vector <double> susag;
	vector <vector <double> > Qma;
	
	susag.resize(data.nage);
	
	for(c = 0; c < data.narea; c++){
		for(a = 0; a < data.nage; a++) susag[a] = 0;
		 
		sum = 0; 
		for(dp = 0; dp < data.ndemocatpos; dp++){
			val = model.sus[dp]*data.area[c].pop[dp];
			susag[data.democatpos[dp][0]] += val;
			sum += val;
		}
		
		for(j = 0; j < indinf[c].size(); j++){
			i = indinf[c][j];
			dp = data.ind[i].dp;
			
			sum -= model.sus[dp];
			susag[data.democatpos[dp][0]] -= model.sus[dp];
		}		
			
		dd = sum - sussum[poptree.level-1][c]; if(dd*dd > tiny) emsg("Part: EC20");
		
		for(dp = 0; dp < data.nage; dp++){
			dd = susag[dp] - susage[c][dp]; 
			if(dd*dd > tiny) emsg("Part: EC21");
		}
	}
		
	Qma.resize(data.narea);
	for(c = 0; c < data.narea; c++){ Qma[c].resize(data.nage); for(a = 0; a < data.nage; a++) Qma[c][a] = 0;}
	
	for(c = 0; c < data.narea; c++){
		for(j = 0; j < indinf[c].size(); j++){
			i = indinf[c][j];
			k = 0; cc = 0; while(k < indev[i].size() && t >= indev[i][k].t){ cc = trans[indev[i][k].trans].to; k++;}
			
			q = 0; while(q < data.Q.size() && !(data.Q[q].comp == comp[cc].name && data.Q[q].timep == indev[i][k].timep)) q++;
			if(q < data.Q.size()){ 			
				inf = comp[cc].infectivity;
				
				dp = data.ind[i].dp;
				a = data.democatpos[dp][0];
				v = c*data.nage + a;
				kmax = data.Q[q].to[v].size();
				for(k = 0; k < kmax; k++){
					cc = data.Q[q].to[v][k];
					for(aa = 0; aa < data.nage; aa++){
						Qma[cc][aa] += model.areafac[cc]*inf*data.Q[q].val[v][k][aa];
					}
				}
			}
		}
	}
		
	for(c = 0; c < data.narea; c++){
		for(a = 0; a < data.nage; a++){
			dd = Qma[c][a] - Qmap[c][a]; if(dd*dd > tiny) emsg("Part: EC22");
		}
	}
	
	sum = 0; 
	for(c = 0; c < data.narea; c++){
		sum2 = 0; for(a = 0; a < data.nage; a++) sum2 += Qma[c][a]*susage[c][a];	
		dd = sum2 - Rtot[poptree.level-1][c]; if(dd*dd > tiny) emsg("Part: EC22b");
		sum += sum2;
	}
	
	for(l = 0; l < poptree.level; l++){
		cmax = lev[l].add.size();
		sum2 = 0; for(c = 0; c < cmax; c++) sum2 += Rtot[l][c];
		dd = sum - sum2; if(dd*dd > tiny) emsg("Part: EC23");
	}
	
	for(l = 0; l < poptree.level; l++){
		cmax = lev[l].add.size();
		for(c = 0; c < cmax; c++){ if(lev[l].add[c] != 0) emsg("Part: EC7");}
	}	
}

/// Makes changes corresponding to a compartmental transition in one of the individuals
void PART::dofe()
{
	unsigned int i, dq, q, c, cc, ccc, dp, v, a, k, kmax, j, jmax, loop;
	int l;
	double sum, val, fac, fac2;
	TRANS tr;
		 
	i = fev[tdnext][tdfnext].ind; 
	c = data.ind[i].area;
		 
	tr = trans[fev[tdnext][tdfnext].trans];
	N[tr.from]--; if(N[tr.from] < 0) emsg("Part: EC12");
	N[tr.to]++;

	dq = tr.DQ[fev[tdnext][tdfnext].timep]; 
		
	tdfnext++;
	if(tdfnext == fev[tdnext].size()){
		tdnext++; tdfnext = 0; 
		while(tdnext < fediv && fev[tdnext].size() == 0) tdnext++;
	}
		
	if(dq == UNSET) return;
	
	dp = data.ind[i].dp;
	v = c*data.nage+data.democatpos[dp][0];
	
	for(l = 0; l < int(poptree.level); l++) lev[l].donelist.clear();
	
	l = poptree.level-1;                                                             // Makes change to Rtot
	
	for(loop = 0; loop < 2; loop++){
		q = model.DQ[dq].q[loop];
		if(q != UNSET){
			fac = model.DQ[dq].fac[loop];
			
			kmax = data.Q[q].to[v].size();
			for(k = 0; k < kmax; k++){
				cc = data.Q[q].to[v][k];
				fac2 = fac*model.areafac[cc];
				
				sum = 0;
				for(a = 0; a < data.nage; a++){
					val = fac2*data.Q[q].val[v][k][a];
					Qmap[cc][a] += val;
					sum += val*susage[cc][a];
				}
				Rtot[l][cc] += sum;
				
				ccc = lev[l].node[cc].parent;
				if(lev[l-1].add[ccc] == 0){ lev[l-1].donelist.push_back(ccc);}		
				lev[l-1].add[ccc] += sum;
			}
		}			
	}
	
	for(l = poptree.level-2; l >= 0; l--){                                        // Propages change up the tree
		jmax = lev[l].donelist.size();
		for(j = 0; j < jmax; j++){
			c = lev[l].donelist[j];
			
			sum = lev[l].add[c];
			Rtot[l][c] += sum;
			lev[l].add[c] = 0;
			 
			if(l > 0){
				cc = lev[l].node[c].parent;
				if(lev[l-1].add[cc] == 0){ lev[l-1].donelist.push_back(cc);}		
				lev[l-1].add[cc] += sum;
			}
		}
	}
}

/// This samples the node on the fine scale in which the next infection occurs
unsigned int PART::nextinfection(unsigned int type)
{
	unsigned int l, lmax, c, cc, j, jmax;
	double z, sum;
	vector <double> sumst;
	
	l = 0; c = 0;                              // We start at the top level l=0 and proceed to fine and finer scales
	lmax = poptree.level;
	while(l < lmax-1){
		jmax = lev[l].node[c].child.size();
		
		sumst.resize(jmax);
		sum = 0;
		for(j = 0; j < jmax; j++){
			cc = lev[l].node[c].child[j];
	
			if(type == INF_EV) sum += Rtot[l+1][cc];
			else sum += sussum[l+1][cc];
				
			sumst[j] = sum;
		}
		
		z = ran()*sum; j = 0; while(j < jmax && z > sumst[j]) j++;
		if(j == jmax) emsg("Part: EC15");
		
		c = lev[l].node[c].child[j];
		l++;
	};
	
	return c;
}

/// Packs up all the particle information (from time fedivmin until the end) to the be sent by MPI
void PART::partpack(unsigned int fedivmin)
{
	packinit();
	pack(indinf);
	pack(Qmap);
	pack(fev,fedivmin,fediv);
	pack(N);
	pack(tdnext); 
	pack(tdfnext);
}

/// Unpacks particle 
void PART::partunpack(unsigned int fedivmin)
{
	unsigned int k, kmax, j, jmax, c, cmax, cc, dp, a;
	int l;
	double val, val2, sus, sum;
	
	packinit();
	unpack(indinf);
	unpack(Qmap);
	unpack(fev,fedivmin,fediv);
	unpack(N);
	unpack(tdnext); 
	unpack(tdfnext);

	sussum = sussumst;
	susage = susagest;
	
	l = poptree.level-1;
	cmax = data.narea;
	for(c = 0; c < cmax; c++){
		kmax = indinf[c].size();
		for(k = 0; k < kmax; k++){
			dp = data.ind[indinf[c][k]].dp;
			sus = model.sus[dp];
			sussum[l][c] -= sus;
			susage[c][data.democatpos[dp][0]] -= sus;
		}
	
		sum = 0; for(a = 0; a < data.nage; a++) sum += susage[c][a]*Qmap[c][a];

		Rtot[l][c] = sum;
	}

	for(l = poptree.level-2; l >= 0; l--){
		cmax = lev[l].node.size();
		for(c = 0; c < cmax; c++){
			val = 0; val2 = 0;
			jmax = lev[l].node[c].child.size();
			for(j = 0; j < jmax; j++){
				cc = lev[l].node[c].child[j];
				val += sussum[l+1][cc];
				val2 += Rtot[l+1][cc];
			}
			sussum[l][c] = val;
			Rtot[l][c] = val2;
		}
	}
}

/// Copies in all the information from another particle
void PART::copy(const PART &other, unsigned int fedivmin)
{
	unsigned int d;
	
	indinf = other.indinf;
	Rtot = other.Rtot; 
	Qmap = other.Qmap; 
	sussum = other.sussum;
	susage = other.susage;
	for(d = fedivmin; d < fediv; d++) fev[d] = other.fev[d];
	N = other.N;
	tdnext = other.tdnext;
	tdfnext = other.tdfnext;
}

/// Adds a future event to the timeline
void PART::addfev(FEV fe, double period, double tnow)
{
	unsigned int d, j, jmax;
	double t;

	t = fe.t; if(t < tnow){ cout << t << " " << tnow << "\n"; emsg("PART: EC10");}
	if(t >= period) return;
	
	d = (unsigned int)((t/period)*fev.size());

	j = 0; jmax = fev[d].size();
	if(t != tnow){ while(j < jmax && t >= fev[d][j].t) j++;}
	else{ while(j < jmax && t > fev[d][j].t) j++;}
	
	if(j == jmax) fev[d].push_back(fe);
	else fev[d].insert(fev[d].begin()+j,fe);
	
	if(t != tnow){
		if(d == tdnext){ if(j < tdfnext) tdfnext = j;}
		if(d < tdnext){ tdnext = d; tdfnext = j;}
	}
	else{
		TRANS tr = trans[fe.trans];
		N[tr.from]--; if(N[tr.from] < 0) emsg("Part: EC12"); 
		N[tr.to]++;
		
		if(d == tdnext) tdfnext++;
	}
}
