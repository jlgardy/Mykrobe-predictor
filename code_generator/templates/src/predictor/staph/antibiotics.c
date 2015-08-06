/*
 * Copyright 2014 Zamin Iqbal (zam@well.ox.ac.uk)
 * 
 *  antibiotics.c 
*/
#include "antibiotics.h"
#include "file_reader.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "mut_models.h"
#include "gene_presence_models.h"
#include "json.h"


{% include 'src/predictor/staph/map_gene_to_drug_resistance.c' %}
{% include 'src/predictor/staph/map_antibiotic_enum_to_str.c' %}






AntibioticInfo* alloc_antibiotic_info()
{
  AntibioticInfo* abi = (AntibioticInfo*) calloc(1, sizeof(AntibioticInfo));
  if (abi==NULL)
    {
      return NULL;
    }
  else
    {
      abi->m_fasta = strbuf_new();
      abi->num_genes = 0;
      abi->vars = (Var**) malloc(sizeof(Var*)*NUM_KNOWN_MUTATIONS);
      if (abi->vars==NULL)
	{
	  strbuf_free(abi->m_fasta);
          free(abi);
	  return NULL;
	}
      abi->genes = (GeneInfo**) malloc(sizeof(GeneInfo*)*NUM_GENE_PRESENCE_GENES);
      if (abi->genes==NULL)
	{
	  free(abi->vars);
	  strbuf_free(abi->m_fasta);
	  free(abi);
	  return NULL;
	}
      abi->which_genes = (int*) calloc(MAX_GENES_PER_ANTIBIOTIC, sizeof(int));
      if (abi->which_genes==NULL)
	{
	  free(abi->genes);
	  free(abi->vars);
	  strbuf_free(abi->m_fasta);
	  free(abi);
	  return NULL;
	  
	}
      int i;
      for (i=0; i<NUM_KNOWN_MUTATIONS; i++)
	{
	  abi->vars[i] = alloc_var();
	  if (abi->vars[i]==NULL)
	    {
	      free(abi->vars);
	      free(abi->genes); 
	      strbuf_free(abi->m_fasta);
	      free(abi);
	      return NULL; //creates a leak if i>0
	    }
	}
      for (i=0; i<NUM_GENE_PRESENCE_GENES; i++)
	{
	  abi->genes[i] = alloc_and_init_gene_info();
	  if (abi->genes[i]==NULL)
	    {
	      free(abi->vars);
              free(abi->genes);
              strbuf_free(abi->m_fasta);
              free(abi);
              return NULL; 
	    }
	  abi->genes[i]->name = (GenePresenceGene) i;
	}

    }
  return abi;
}
void free_antibiotic_info(AntibioticInfo* abi)
{
  if (abi==NULL)
    {
      return;
    }
  else
    {
      strbuf_free(abi->m_fasta);
      int i;
      for (i=0; i<NUM_KNOWN_MUTATIONS; i++)
	{
	  free_var(abi->vars[i]);
	}
      free(abi->vars);
      for (i=0; i<NUM_GENE_PRESENCE_GENES; i++)
	{
	  free_gene_info(abi->genes[i]);
	}
      free(abi->genes);
      free(abi->which_genes);
      free(abi);
    }
}

void reset_antibiotic_info(AntibioticInfo* abi)
{
  abi->ab = NoDrug;
  strbuf_reset(abi->m_fasta);
  abi->num_mutations=0;

  int i;
  for (i=0; i<NUM_KNOWN_MUTATIONS; i++)
    {
      reset_var_on_background(abi->vars[i]->vob_best_sus);
      reset_var_on_background(abi->vars[i]->vob_best_res);
    }
  for (i=0; i<NUM_GENE_PRESENCE_GENES; i++)
    {
      reset_gene_info(abi->genes[i]);
    }
}

void  load_antibiotic_mutation_info_on_sample(FILE* fp,
					      dBGraph* db_graph,
					      int (*file_reader)(FILE * fp, 
								 Sequence * seq, 
								 int max_read_length, 
								 boolean new_entry, 
								 boolean * full_entry),
					      AntibioticInfo* abi,
					      ReadingUtils* rutils,
					      VarOnBackground* tmp_vob,	
					      int ignore_first, int ignore_last, int expected_covg)
{
  reset_reading_utils(rutils);
  reset_var_on_background(tmp_vob);

  StrBuf* tmp1 = strbuf_new();
  StrBuf* tmp2 = strbuf_new();
  StrBuf* tmp3 = strbuf_new();

  
  KnownMutation m = NotSpecified;
  boolean ret=true;

  while (ret==true)
    {
      ret = get_next_var_on_background(fp, 
				       db_graph, 
				       tmp_vob, abi->vars,
				       rutils->seq, 
				       rutils->kmer_window, 
				       file_reader,
				       rutils->array_nodes, 
				       rutils->array_or,
				       rutils->working_ca, 
				       MAX_LEN_MUT_ALLELE,
				       tmp1, tmp2, tmp3,
				       ignore_first, ignore_last, 
				       expected_covg, &m);


    }
  
  strbuf_free(tmp1);
  strbuf_free(tmp2);
  strbuf_free(tmp3);

}


//one gene per fasta, so if you have multiple reads,
//these are different exemplars, for divergent versions of the same gene
void load_antibiotic_gene_presence_info_on_sample(FILE* fp,
						  dBGraph* db_graph,
						  int (*file_reader)(FILE * fp, 
								     Sequence * seq, 
								     int max_read_length, 
								     boolean new_entry, 
								     boolean * full_entry),
						  AntibioticInfo* abi,
						  ReadingUtils* rutils,
						  GeneInfo* tmp_gi)



{
  reset_reading_utils(rutils);

  int num=1;

  while (num>0)
    {
      num = get_next_gene_info(fp,
			       db_graph,
			       tmp_gi,
			       rutils->seq,
			       rutils->kmer_window,
			       file_reader,
			       rutils->array_nodes,
			       rutils->array_or,
			       rutils->working_ca,
			       MAX_LEN_GENE);
      /*      printf("Percent >0 %d\n Median on nonzero %d\nMin %d\n, median %d\n",
	     tmp_gi->percent_nonzero,
	     tmp_gi->median_covg_on_nonzero_nodes,
	     tmp_gi->median_covg,
	     tmp_gi->min_covg);
      */

      if (tmp_gi->percent_nonzero>abi->genes[tmp_gi->name]->percent_nonzero)
	{
	  copy_gene_info(tmp_gi, abi->genes[tmp_gi->name]);
	}

    }
  

}



void load_antibiotic_mut_and_gene_info(dBGraph* db_graph,
				       int (*file_reader)(FILE * fp, 
							  Sequence * seq, 
							  int max_read_length, 
							  boolean new_entry, 
							  boolean * full_entry),
				       AntibioticInfo* abi,
				       ReadingUtils* rutils,
				       VarOnBackground* tmp_vob,
				       GeneInfo* tmp_gi,
				       int ignore_first, 
				       int ignore_last, 
				       int expected_covg,
				       StrBuf* install_dir)

{

  FILE* fp;

  if (abi->num_mutations>0)
    {
      fp = fopen(abi->m_fasta->buff, "r");
      if (fp==NULL)
	{
	  die("Cannot open %s - should be there as part of the install - did you run out of disk mid-install?\n",
	      abi->m_fasta->buff);
	}
      
      load_antibiotic_mutation_info_on_sample(fp,
					      db_graph,
					      file_reader,
					      abi,
					      rutils, 
					      tmp_vob,
					      ignore_first, ignore_last, expected_covg);
      fclose(fp);
    }
  if (abi->num_genes>0)
    {
      StrBuf* tmp = strbuf_new();
      int j;
      for (j=0; j<abi->num_genes; j++)
	{
	  strbuf_reset(tmp);
	  GenePresenceGene g = (GenePresenceGene) abi->which_genes[j];
	  map_gene_to_fasta(g, tmp, install_dir);
	  FILE* fp = fopen(tmp->buff, "r");
	  if (fp==NULL)
	    {
	      die("Unable to open %s, which should come as part of the install.\n", tmp->buff);
	    }
	  load_antibiotic_gene_presence_info_on_sample(fp,
						       db_graph,
						       file_reader,
						       abi,
						       rutils,
						       tmp_gi);
	  fclose(fp);
	}
      strbuf_free(tmp);

    }
}




void update_infection_type(InfectionType* I_new, InfectionType* I_permanent){
  if ( (*I_permanent==Unsure) || (*I_permanent==Susceptible) ){
    *I_permanent = *I_new;
  }
}

{% for drug in selfer.drugs %}
{% include 'src/predictor/staph/is_drug_susceptible.c' %}
{% endfor %}


void print_antibiotic_susceptibility(dBGraph* db_graph,
					int (*file_reader)(FILE * fp, 
							   Sequence * seq, 
							   int max_read_length, 
							   boolean new_entry, 
							   boolean * full_entry),
					ReadingUtils* rutils,
					VarOnBackground* tmp_vob,
					GeneInfo* tmp_gi,
					AntibioticInfo* abi,
					InfectionType (*func)(dBGraph* db_graph,
							int (*file_reader)(FILE * fp, 
									   Sequence * seq, 
									   int max_read_length, 
									   boolean new_entry, 
									   boolean * full_entry),
      							ReadingUtils* rutils,
      							VarOnBackground* tmp_vob,
      							GeneInfo* tmp_gi,
      							AntibioticInfo* abi,
      							StrBuf* install_dir,
      							int ignore_first,
                    int ignore_last,
                    int expected_covg,
      							double lambda_g,
                    double lambda_e,
                    double err_rate,
                    CalledVariant* called_variants,
                    CalledGene* called_genes,
                    CmdLine* cmd_line),
					StrBuf* tmpbuf,
					StrBuf* install_dir,
					int ignore_first, int ignore_last,
					int expected_covg,
					double lambda_g, double lambda_e, double err_rate,
          CmdLine* cmd_line,
					boolean output_last,//for JSON,
          CalledVariant* called_variants,
          CalledGene* called_genes
					)
{
  InfectionType suc;
  
  suc  = func(db_graph,
	      file_reader,
	      rutils,
	      tmp_vob,
	      tmp_gi,
	      abi, 
	      install_dir,
	      ignore_first, 
	      ignore_last, 
	      expected_covg,
	      lambda_g,
	      lambda_e,
	      err_rate,
        called_variants,
        called_genes,
        cmd_line);

  
  map_antibiotic_enum_to_str(abi->ab, tmpbuf);
  if (cmd_line->format==Stdout)
    {
      printf("%s\t", tmpbuf->buff);
      if (suc==Susceptible)
	{
	  printf("S\n");
	}
      else if (suc==MixedInfection)
	{
	  printf("r\n");
	}
      else if (suc==Resistant)
	{
	  printf("R\n");
	}
      else
	{
	  printf("N\n");
	}
    }
  else
    {
      if (suc==Susceptible)
	{
	    print_json_item(tmpbuf->buff, "S", output_last);
	}
      else if ( suc==Resistant )
	{
	  print_json_item(tmpbuf->buff, "R", output_last);
	}
      else if ( suc==MixedInfection )
  {
    print_json_item(tmpbuf->buff, "r", output_last);
  }  
      else
	{
	  print_json_item(tmpbuf->buff, "Inconclusive", output_last);
	}
    }

}


void print_erythromycin_susceptibility(dBGraph* db_graph,
					  int (*file_reader)(FILE * fp, 
							     Sequence * seq, 
							     int max_read_length, 
							     boolean new_entry, 
							     boolean * full_entry),
					  ReadingUtils* rutils,
					  VarOnBackground* tmp_vob,
					  GeneInfo* tmp_gi,
					  AntibioticInfo* abi,
					  InfectionType (*func)(dBGraph* db_graph,
							 int (*file_reader)(FILE * fp, 
									    Sequence * seq, 
									    int max_read_length, 
									    boolean new_entry, 
									    boolean * full_entry),
							  ReadingUtils* rutils,
							  VarOnBackground* tmp_vob,
							  GeneInfo* tmp_gi,
							  AntibioticInfo* abi,
							  StrBuf* install_dir,
							  int ignore_first, int ignore_last, int expected_covg,
							  double lambda_g, double lambda_e, double err_rate, 
								boolean* any_erm_present, 
                CalledVariant* called_variants,CalledGene* called_genes,
                CmdLine* cmd_line),
					  StrBuf* tmpbuf,
					  StrBuf* install_dir,
					  int ignore_first, int ignore_last, int expected_covg,
					  double lambda_g, double lambda_e, double err_rate, CmdLine* cmd_line, boolean output_last,//for JSON 
					  boolean* any_erm_present, InfectionType* erythromycin_resistotype, 
            CalledVariant* called_variants,CalledGene* called_genes
					 )
{
  InfectionType suc;
  
  suc  = func(db_graph,
	      file_reader,
	      rutils,
	      tmp_vob,
	      tmp_gi,
	      abi,
	      install_dir,
	      ignore_first, ignore_last, expected_covg,
	      lambda_g,
	      lambda_e,
	      err_rate,
	      any_erm_present,
        called_variants,
         called_genes,
         cmd_line);

  map_antibiotic_enum_to_str(abi->ab, tmpbuf);
  *erythromycin_resistotype = suc;
  
  if (cmd_line->format==Stdout)
    {
      printf("%s\t", tmpbuf->buff);
      if (suc==Susceptible)
	{
	  printf("S\n");
	}
      else if (suc==MixedInfection)
	{
	  printf("r\n");
	}
      else if (suc==Resistant)
	{
	  printf("R\n");
	}
      else
	{
	  printf("N\n");
	}
    }
  else
    {
      if (suc==Susceptible)
	{
	  print_json_item(tmpbuf->buff, "S", output_last);
	}
      else if ( suc==Resistant)
	{
	  print_json_item(tmpbuf->buff, "R", output_last);
	}
      else if ( suc==MixedInfection ) 
  {
    print_json_item(tmpbuf->buff, "r", output_last);
  }
      else
	{
	  print_json_item(tmpbuf->buff, "Inconclusive", output_last);
	}
    }

}


void print_clindamycin_susceptibility(dBGraph* db_graph,
					 int (*file_reader)(FILE * fp, 
							    Sequence * seq, 
							    int max_read_length, 
							    boolean new_entry, 
							    boolean * full_entry),
					 ReadingUtils* rutils,
					 VarOnBackground* tmp_vob,
					 GeneInfo* tmp_gi,
					 AntibioticInfo* abi,
					 InfectionType (*func)(dBGraph* db_graph,
							 int (*file_reader)(FILE * fp, 
									    Sequence * seq, 
									    int max_read_length, 
									    boolean new_entry, 
									    boolean * full_entry),
							 ReadingUtils* rutils,
							 VarOnBackground* tmp_vob,
							 GeneInfo* tmp_gi,
							 AntibioticInfo* abi,
							 StrBuf* install_dir,
							 int ignore_first, int ignore_last, int expected_covg,
							 double lambda_g, double lambda_e, double err_rate,
               CalledVariant* called_variants,CalledGene* called_genes,
               CmdLine* cmd_line),
					 StrBuf* tmpbuf,
					 boolean any_erm_present, InfectionType erythromycin_resistotype,
					 StrBuf* install_dir,
					 int ignore_first, int ignore_last, int expected_covg,
					 double lambda_g, double lambda_e, double err_rate, CmdLine* cmd_line, boolean output_last,
           CalledVariant* called_variants,CalledGene* called_genes//for JSON 
					 )
{
  InfectionType suc;
  
  suc  = func(db_graph,
	      file_reader,
	      rutils,
	      tmp_vob,
	      tmp_gi,
	      abi,
	      install_dir,
	      ignore_first, ignore_last, expected_covg,
	      lambda_g,
	      lambda_e,
	      err_rate,
        called_variants,
        called_genes,
        cmd_line);


  map_antibiotic_enum_to_str(abi->ab, tmpbuf);


    if (suc==Resistant)
	{
	  print_json_item(tmpbuf->buff, "R(constitutive)", output_last);
	}
      else if (suc==MixedInfection)
	{
	  print_json_item(tmpbuf->buff, "r(constitutive)", output_last);
	}
      else if ( (suc==Susceptible) && (any_erm_present==true) )
	{
	    if (erythromycin_resistotype == Resistant){
	      print_json_item(tmpbuf->buff, "R(inducible)", output_last);
	    }
	    else if (erythromycin_resistotype == MixedInfection){
	      print_json_item(tmpbuf->buff, "r(inducible)", output_last);
	    }	
	}
      else if (suc==Susceptible)
	{
	  print_json_item(tmpbuf->buff, "S", output_last);
	}
      else
	{
	  print_json_item(tmpbuf->buff, "Inconclusive", output_last);
	}

}




///virulence




{% for gene in selfer.virulence_genes %}
{% with %}
    {% set loop_last = loop.last %}
	{% include 'src/predictor/staph/is_virulence_gene_positive.c' %}
	{% include 'src/predictor/staph/print_virulence_gene_presence.c' %}
{% endwith %}
{% endfor %}


