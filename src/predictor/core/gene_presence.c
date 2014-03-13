/*
 * Copyright 2014 Zamin Iqbal (zam@well.ox.ac.uk)
 * 
 *
 * **********************************************************************
 *
 * This file is part of myKrobe.
 *
 * myKrobe is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * myKrobe is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with myKrobe.  If not, see <http://www.gnu.org/licenses/>.
 *
 * **********************************************************************
 */
/*
  gene_presence.c
*/


#include "gene_presence.h"
#include "element.h"
#include "file_reader.h"
#include "build.h"
#include "db_differentiation.h"
#include "maths.h"
#include "string_buffer.h"

GenePresenceGene map_string_to_gene_presence_gene(StrBuf* sbuf)
{
  if (strcmp(sbuf->buff, "aacA-aphD")==0)
    {
      return aacAaphD;
    }
  else if (strcmp(sbuf->buff, "blaZ")==0)
    {
      return blaZ ;
    }
  else if (strcmp(sbuf->buff, "dfrA")==0)
    {
      return dfrA;
    }
  else if (strcmp(sbuf->buff, "dfrG")==0)
    {
      return dfrG ;
    }
  else if (strcmp(sbuf->buff, "ermA")==0)
    {
      return ermA;
    }
  else if (strcmp(sbuf->buff, "ermB")==0)
    {
      return ermB;
    }
  else if (strcmp(sbuf->buff, "ermC")==0)
    {
      return ermC;
    }
  else if (strcmp(sbuf->buff, "ermT")==0)
    {
      return ermT;
    }
  else if (strcmp(sbuf->buff, "fusB")==0)
    {
      return fusB;
    }
  else if (strcmp(sbuf->buff, "fusC")==0)
    {
      return fusC;
    }
  else if (strcmp(sbuf->buff, "vga(A)LC")==0)
    {
      return vga_A_LC;
    }
  else if (strcmp(sbuf->buff, "msrA")==0)
    {
      return msrA;
    }
  else if (strcmp(sbuf->buff, "mecA")==0)
    {
      return mecA;
    }
  else if (strcmp(sbuf->buff, "tetK")==0)
    {
      return tetK;
    }
  else if (strcmp(sbuf->buff, "tetL")==0)
    {
      return tetL;
    }
  else if (strcmp(sbuf->buff, "tetM")==0)
    {
      return tetM;
    }
  else if (strcmp(sbuf->buff, "vanA")==0)
    {
      return vanA;
    }
  else if (strcmp(sbuf->buff, "mupA")==0)
    {
      return mupA;
    }
  else if (strcmp(sbuf->buff, "mupB")==0)
    {
      return mupB;
    }

  else 
    {
      die("Unknown gene %s\n", sbuf->buff);
    }

}


GeneInfo* alloc_and_init_gene_info()
{
  GeneInfo* gi = (GeneInfo*) calloc(1, sizeof(GeneInfo));
  if (gi==NULL)
    {
      die("Can't alloc gi\n");
    }
  gi->strbuf=strbuf_new(); 
  gi->name = unspecified_gpg;
  return gi;
}
void free_gene_info(GeneInfo* gi)
{
  strbuf_free(gi->strbuf);
  free(gi);
}
void copy_gene_info(GeneInfo* from_gi, GeneInfo* to_gi)
{
  to_gi->median_covg = from_gi->median_covg;
  to_gi->min_covg = from_gi->min_covg;
  to_gi->percent_nonzero = from_gi->percent_nonzero;
  strbuf_reset(to_gi->strbuf);
  strbuf_append_str(to_gi->strbuf, from_gi->strbuf->buff);
  to_gi->name = from_gi->name;
}

void reset_gene_info(GeneInfo* gi)
{
  if (gi==NULL)
    {
      return;
    }
  gi->median_covg=0;
  gi->min_covg=0;
  gi->percent_nonzero=0;
  if (gi->strbuf!=NULL)
    {
      strbuf_reset(gi->strbuf);
    }
  gi->name = unspecified_gpg;
}

//assume you are going to repeatedly call this on a fasta
//file of genes
int get_next_gene_info(FILE* fp, 
		       dBGraph* db_graph, 
		       GeneInfo* gene_info,
		       Sequence* seq, 
		       KmerSlidingWindow* kmer_window,
		       int (*file_reader)(FILE * fp, 
					  Sequence * seq, 
					  int max_read_length, 
					  boolean new_entry, 
					  boolean * full_entry),
		       dBNode** array_nodes, 
		       Orientation*  array_or,
		       CovgArray* working_ca, 
		       int max_read_length)

{

  boolean full_entry=true;
  int dummy_colour_ignored=0;
  int num_kmers = 
    align_next_read_to_graph_and_return_node_array(fp, 
						   max_read_length, 
						   array_nodes, 
						   array_or, 
						   false, 
						   &full_entry, 
						   file_reader,
						   seq, 
						   kmer_window, 
						   db_graph, 
						   dummy_colour_ignored);

  if (num_kmers==0)
    {
      return 0;
    }
  //collect min, median covg on gene and also percentage of kmers with any covg
  boolean too_short=false;
  int ignore_first=0;
  int ignore_last=0;
  gene_info->median_covg = 
    median_covg_on_allele_in_specific_colour(array_nodes,
					     num_kmers,
					     working_ca,
					     0,
					     &too_short,
					     ignore_first, ignore_last);
  gene_info->min_covg = 
    min_covg_on_allele_in_specific_colour(array_nodes,
					  num_kmers,
					  0,
					  &too_short,
					  ignore_first, ignore_last);
  gene_info->percent_nonzero = 
    percent_nonzero_on_allele_in_specific_colour(array_nodes,
						 num_kmers,
						 0,
						 &too_short,
						 ignore_first, ignore_last);
  strbuf_reset(gene_info->strbuf);
  strbuf_append_str(gene_info->strbuf,
		    seq->name);
  gene_info->name 
    = map_string_to_gene_presence_gene(gene_info->strbuf);

  return num_kmers;
}
