#include <math.h>
#include <stdlib.h>
#include "quac.h"
#include "operators.h"
#include "solver.h"
#include "petsc.h"
#include "dm_utilities.h"

PetscErrorCode ts_monitor(TS,PetscInt,PetscReal,Vec,void*);
FILE *f_pop;

int main(int argc,char **args){
  PetscInt N_th,num_phonon,N_th2,num_phonon2,init_phonon,steady_state_solve,steps_max;
  PetscReal time_max,dt,gam_res,gam_eff,gam_dep;
  double w_m,D_e,Omega,gamma_eff,lambda_eff,lambda_s,gamma_par,purcell_f,gamma_res,gamma_dep,hbar;
  double Q,alpha,kHz,MHz,GHz,THz,Hz,rate;
  operator a,nv;
  Vec      rho;


  /* Initialize QuaC */
  QuaC_initialize(argc,args);


  /* Define units */
  GHz = 1e3;
  MHz = GHz*1e-3;
  kHz = MHz*1e-3;
  THz = GHz*1e3;
  Hz  = kHz*1e-3;

  N_th       = 5;
  N_th2      = 10;
  num_phonon = 50;
  num_phonon2 = 50;
  init_phonon = 12;
  steady_state_solve = 1;
  gam_res = 1.0;
  gam_eff = 1.0;
  gam_dep = 0.0;

  /* Get arguments from command line */
  PetscOptionsGetInt(NULL,NULL,"-num_phonon",&num_phonon,NULL);
  PetscOptionsGetInt(NULL,NULL,"-n_th",&N_th,NULL);
  PetscOptionsGetInt(NULL,NULL,"-num_phonon2",&num_phonon2,NULL);
  PetscOptionsGetInt(NULL,NULL,"-n_th2",&N_th2,NULL);
  PetscOptionsGetInt(NULL,NULL,"-init_phonon",&init_phonon,NULL);
  PetscOptionsGetInt(NULL,NULL,"-steady_state",&steady_state_solve,NULL);
  PetscOptionsGetReal(NULL,NULL,"-gam_res",&gam_res,NULL);
  PetscOptionsGetReal(NULL,NULL,"-gam_eff",&gam_eff,NULL);
  PetscOptionsGetReal(NULL,NULL,"-gam_dep",&gam_dep,NULL);

  if (nid==0) printf("Num_phonon: %ld N_th: %ld Num_phonon2: %ld N_th2: %ld gam_res: %f gam_eff %f gam_dep %f\n",num_phonon,N_th,num_phonon2,N_th2,gam_res,gam_eff,gam_dep);
  /* Define scalars to add to Ham */
  w_m        = 175*MHz*2*M_PI; //Mechanical resonator frequency
  lambda_s   = 0.1*MHz*2*M_PI;
  gamma_eff  = lambda_s*gam_eff;//1*MHz; //Effective dissipation rate
  gamma_res  = lambda_s*gam_res;
  gamma_dep  = lambda_s*gam_dep;

  print_dense_ham();

  create_op(num_phonon,&a);
  create_op(num_phonon2,&nv);

  /* Add terms to the hamiltonian */
  add_to_ham(w_m,a->n); // w_m at a

  add_to_ham(w_m,nv->n); // w_m nvt n

  /* Below 4 terms represent lambda_s (nvt + nv)(at + a) */
  //  add_to_ham_mult2(lambda_s,a->dag,nv->dag); //nvt at
  add_to_ham_mult2(lambda_s,nv->dag,a);  //nvt a
  add_to_ham_mult2(lambda_s,nv,a->dag);  //nv at
 //  add_to_ham_mult2(lambda_s,nv,a);   //nv a

  /* nv center lindblad terms */
  rate = gamma_dep*(N_th2+1);
  add_lin(rate,a);

  rate = gamma_dep*(N_th2);
  add_lin(rate,a->dag);

  /* phonon bath thermal terms */
  rate = gamma_res*(N_th+1);
  add_lin(rate,a);

  rate = gamma_res*(N_th);
  add_lin(rate,a->dag);

  create_full_dm(&rho);

  if (steady_state_solve==1) {
    steady_state(rho);
    purcell_f = 4*lambda_s*lambda_s/(gamma_eff+gamma_res+0.5*gamma_dep);
    rate = gamma_res/(gamma_res+purcell_f/(1+purcell_f/gamma_eff));
    if(nid==0) printf("Predicted cooling fraction: %f\n",rate);
  } else {
    set_ts_monitor(ts_monitor);
    if (nid==0){
      f_pop = fopen("pop","w");
      fprintf(f_pop,"#Time Populations\n");
    }

    set_initial_pop(a,init_phonon);
    set_initial_pop(nv,init_phonon);
    set_dm_from_initial_pop(rho);
    time_max  = 150;
    dt        = 0.01;
    steps_max = 10000;
    time_step(rho,0.0,time_max,dt,steps_max);
  }

  destroy_op(&a);
  destroy_op(&nv);
  destroy_dm(rho);
  QuaC_finalize();
  return 0;
}
PetscErrorCode ts_monitor(TS ts,PetscInt step,PetscReal time,Vec dm,void *ctx){
  double hbar,*populations;
  hbar = 6.58211e-7;
  int num_pop,i;

  num_pop = get_num_populations();
  populations = malloc(num_pop*sizeof(double));
  get_populations(dm,&populations);

  if (nid==0){
    /* Print populations to file */
    fprintf(f_pop,"%e",time);
    for(i=0;i<num_pop;i++){
      fprintf(f_pop," %e ",populations[i]);
    }
    fprintf(f_pop,"\n");
  }
  free(populations);

  PetscFunctionReturn(0);

}
