/* ---------------------------------------------------------------------------
 **    Filename: olap_search.c
 **
 **     License: This file is part of OLAP PROJECT.
 **
 **              OLAP PROJECT is free software: you can redistribute it
 **              and/or modify it under the terms of the GNU General Public
 **              License as published by the Free Software Foundation,
 **              either version 3 of the License, or (at your option)
 **              any later version.
 **
 **              OLAP is distributed in the hope that it will be useful,
 **              but WITHOUT ANY WARRANTY; without even the implied warranty of
 **              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **              GNU General Public License for more details.
 **
 **              You should have received a copy of the GNU General Public
 **              License along with OLAP.
 **              If not, see <http://www.gnu.org/licenses/>.
 **
 ** Description: The proposed file focus on a typed linear algebra approach,
 **              encoding OLAP functionality solely in terms of Linear Algebra
 **              operations, represented in the CSR and BSR format, and
 **              recurring to Intel MKL and GLIB libraries.
 **
 **     Authors: Filipe Oliveira <a57816@alunos.uminho.pt>
 **          and Sérgio Caldas   <a57779@alunos.uminho.pt>
 **
 ** University of Minho, High Performance Computing Dpt. , April 2016
 ** -------------------------------------------------------------------------*/

#ifndef _olap_c
#define _olap_c

#include <stdio.h>
#include <assert.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mkl_spblas.h"
#include "mkl_types.h"
#include "mkl.h"
#include "olap_search.h"


////////////////////////////////////// AUX /////////////////////////////////////
////////////////////////////////////// AUX /////////////////////////////////////
////////////////////////////////////// AUX /////////////////////////////////////

//starts at position 1
char* getfield( char* line, int num, char* return_string ){
  return_string = strtok(line, "|\n");
  int pos = 1;
  for ( ; pos < num; pos++ ){
    return_string = strtok(NULL, "|\n");
  }
  return return_string;
}

void print_csc(
    float* csc_values, MKL_INT* JA1, MKL_INT* IA1,
    MKL_INT NNZ, MKL_INT number_rows, MKL_INT number_columns
    ){

  printf("N NONZ: %d\t", NNZ);
  printf("N ROWS: %d\t", number_rows);
  printf("N COLS: %d\n", number_columns);
  printf("CSC VALUES(%d):\t", sizeof(csc_values));
  for (MKL_INT pos = 0; pos < NNZ; pos++){
    printf("%f, ", csc_values[pos]);
  }
  printf("\nJA1:\t");

  for (int pos = 0; pos < NNZ; pos++){
    printf("%d, ", JA1[pos]);
  }

  printf("\nIA1:\t");
  for (int pos = 0; pos <= number_columns; pos++){
    printf("%d, ", IA1[pos]);
  }
  printf("\n");
}

void print_csr(
    float* csr_values, MKL_INT* JA, MKL_INT* IA,
    MKL_INT NNZ, MKL_INT number_rows, MKL_INT number_columns
    ){

  printf("N NONZ: %d\t", NNZ);
  printf("N ROWS: %d\t", number_rows);
  printf("N COLS: %d\n", number_columns);
  printf("CSR VALUES(%d):\t", sizeof(csr_values));
  for (MKL_INT pos = 0; pos < NNZ; pos++){
    printf("%f, ", csr_values[pos]);
  }
  printf("\nJA:\t");

  for (int pos = 0; pos < NNZ; pos++){
    printf("%d, ", JA[pos]);
  }

  printf("\nIA:\t");
  for (int pos = 0; pos <= number_rows; pos++){
    printf("%d, ", IA[pos]);
  }
  printf("\n");
}

void convert_and_write_to_csv (
    char* filename,
    float* csr_values, MKL_INT* JA, MKL_INT* IA,
    MKL_INT NNZ, MKL_INT number_rows, MKL_INT number_columns
    ){

  MKL_INT job[8];

  //define COO sparse-matrix M
  float* coo_values;
  MKL_INT* coo_rows;
  MKL_INT* coo_columns;

  coo_values = (float*) mkl_malloc (((NNZ+1) * sizeof(float)), MEM_LINE_SIZE );
  coo_rows =  (MKL_INT*) mkl_malloc (((NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  coo_columns =  (MKL_INT*) mkl_malloc (((NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  assert(coo_values != NULL);
  assert(coo_rows != NULL);
  assert(coo_columns != NULL);

  /////////////////////////////////
  //   CONVERT FROM COO TO CSR
  /////////////////////////////////

  // if job[0]=2, the matrix in the coordinate format is converted to the CSR
  // format, and the column indices in CSR representation are sorted in the
  // increasing order within each row.
  job[0]= 0;

  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  job[1]= 0;

  // If job[2]=0, zero-based indexing for the matrix in coordinate format is used;
  job[2]= 0;

  job[3]= 0;
  // job[4]=nzmax - maximum number of the non-zero elements allowed if
  // job[0]=0.
  job[4]= NNZ;

  // If job[5]=3, all arrays rowind, colind, acoo are filled in for the output storage.
  job[5]= 3;

  sparse_status_t status_csr_coo;
  mkl_scsrcoo (job, &number_rows, csr_values, JA, IA, &NNZ, coo_values, coo_rows, coo_columns, &status_csr_coo);

  FILE* stream = fopen(filename, "w+");
  if (stream != NULL ){
    for (int pos = 0; pos < NNZ; pos++){
      fprintf( stream, "%d, %d, %f\n", coo_rows[pos], coo_columns[pos], coo_values[pos]);
    }
    fclose(stream);
  }
}

void read_from_mx (
    char* filename,
    float** A_csr_values, MKL_INT** A_JA, MKL_INT** A_IA,
    MKL_INT* nnz, MKL_INT* rows, MKL_INT* columns
    ){

  __declspec(align(MEM_LINE_SIZE)) MKL_INT current_values_size = ARRAY_SIZE;
  //define COO sparse-matrix M
  __declspec(align(MEM_LINE_SIZE)) MKL_INT* aux_coo_rows;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT* aux_coo_columns;
  __declspec(align(MEM_LINE_SIZE)) float* aux_coo_values;

  aux_coo_rows = (MKL_INT*) malloc (current_values_size * sizeof(MKL_INT));
  aux_coo_columns = (MKL_INT*) malloc (current_values_size * sizeof(MKL_INT));
  aux_coo_values = (float*) malloc (current_values_size * sizeof(float));

  FILE* stream = fopen(filename, "r");
  __declspec(align(MEM_LINE_SIZE)) MKL_INT number_rows = - 1;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT number_columns = -1 ;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT element_number = 1;
  MKL_INT job[8];
  __declspec(align(MEM_LINE_SIZE)) MKL_INT row;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT column;
  float value;
  char line[1024];
  for( element_number = 0 ; (fgets(line, MAX_REG_SIZE, stream) ) ; ++element_number ){
    sscanf(line, "%d, %d, %f\n", &row, &column, &value);
    if ( element_number >= current_values_size ){
      current_values_size *= GROWTH_FACTOR;
      aux_coo_rows = (MKL_INT*) realloc(aux_coo_rows, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
      aux_coo_columns = (MKL_INT*) realloc(aux_coo_columns, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
      aux_coo_values = (float*) realloc(aux_coo_values, (current_values_size) * GROWTH_FACTOR * sizeof(float) );
    }

    /* normal coo property */
    aux_coo_rows[element_number] = row;
    aux_coo_columns[element_number] = column;
    aux_coo_values[element_number] = value;
  }


  fclose(stream);

  MKL_INT NNZ = element_number;
  number_columns = element_number;

  //define COO sparse-matrix M
  __declspec(align(MEM_LINE_SIZE)) float* coo_values;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT* coo_rows;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* coo_columns;

  coo_values = (float*) mkl_malloc (((NNZ+1) * sizeof(float)), MEM_LINE_SIZE );
  coo_rows =  (MKL_INT*) mkl_malloc (((NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  coo_columns =  (MKL_INT*) mkl_malloc (((NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  assert(coo_values != NULL);
  assert(coo_rows != NULL);
  assert(coo_columns != NULL);

  for (int pos = 0; pos < NNZ; pos++) {
    coo_values[pos] = aux_coo_values[pos];
    coo_columns[pos] = aux_coo_columns[pos];
    coo_rows[pos] = aux_coo_rows[pos];
  }

  coo_values[NNZ] = 0.0;
  coo_columns[NNZ] = NNZ;
  coo_rows[NNZ] = NNZ;
  number_rows = NNZ;
  NNZ++;

  //  free(aux_coo_rows);

  /////////////////////////////////
  //   CONVERT FROM COO TO CSR
  /////////////////////////////////

  // if job[0]=2, the matrix in the coordinate format is converted to the CSR
  // format, and the column indices in CSR representation are sorted in the
  // increasing order within each row.
  job[0]= 2;

  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  job[1]= 0;

  // If job[2]=0, zero-based indexing for the matrix in coordinate format is used;
  job[2]= 0;

  job[3]= 0;
  // job[4]=nzmax - maximum number of the non-zero elements allowed if
  // job[0]=0.
  job[4]= NNZ;

  // If job[5]=0, all arrays acsr, ja, ia are filled in for the output storage.
  job[5]= 0;

  *A_csr_values = (float*) mkl_malloc ((NNZ * sizeof(float)), MEM_LINE_SIZE );
  *A_JA = (MKL_INT*) mkl_malloc (( NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *A_IA = (MKL_INT*) mkl_malloc (((number_rows+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  sparse_status_t status_coo_csr;
  mkl_scsrcoo (job, &number_rows, *A_csr_values, *A_JA, *A_IA, &NNZ, coo_values, coo_rows, coo_columns, &status_coo_csr);
  mkl_free(coo_values);
  mkl_free(coo_rows);
  mkl_free(coo_columns);
  *rows = number_rows;
  *columns = number_columns;
  *nnz = NNZ;

}


void tbl_read(
    char* table_name, MKL_INT tbl_column,
    MKL_INT* nnz, MKL_INT* rows, MKL_INT* columns,
    float** A_csr_values, MKL_INT** A_JA, MKL_INT** A_IA,
    MKL_INT **quark_start_end, MKL_INT* quark_global_pos
    ){

  __declspec(align(MEM_LINE_SIZE)) MKL_INT current_values_size = ARRAY_SIZE;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT padding_quark = 0;
  //define COO sparse-matrix M
  __declspec(align(MEM_LINE_SIZE)) MKL_INT* aux_coo_rows;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT* aux_coo_columns;
  __declspec(align(MEM_LINE_SIZE)) float* aux_coo_values;

  aux_coo_rows = (MKL_INT*) malloc (current_values_size * sizeof(MKL_INT));
  aux_coo_columns = (MKL_INT*) malloc (current_values_size * sizeof(MKL_INT));
  aux_coo_values = (float*) malloc (current_values_size * sizeof(float));

  FILE* stream = fopen(table_name, "r");
  __declspec(align(MEM_LINE_SIZE)) MKL_INT number_rows = - 1;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT number_columns = -1 ;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT element_number = 1;
  MKL_INT job[8];
  __declspec(align(MEM_LINE_SIZE)) MKL_INT row;
  __declspec(align(MEM_LINE_SIZE)) MKL_INT column;
  float value;
  char line[1024];

  // get to know how many quarks were used before
  MKL_INT array_pos = *quark_global_pos;
  MKL_INT initial_quark = *quark_start_end[array_pos];
  if (initial_quark > 1 ){
    printf( "%d tables already present in quarks, corresponding to a total of %d\n",array_pos + 1,  initial_quark);
    padding_quark = initial_quark;
  }
  MKL_INT quark_field;
  MKL_INT current_quark;
  MKL_INT global_quark;
  global_quark = padding_quark;
  MKL_INT current_major_row;

  float element_value = 0.0;
  char *field = (char*) malloc( MAX_FIELD_SIZE * sizeof(char) );


  for( element_number = 0 ; (fgets(line, MAX_REG_SIZE, stream) ) ; ++element_number ){
    char* tmp_field = strdup(line);

    field = getfield(tmp_field, tbl_column, field);
    element_value = atof(field);
    current_quark = g_quark_from_string (field);
    quark_field = current_quark - padding_quark;

    // for the quark start end array
    if ( global_quark < current_quark ){
      global_quark = current_quark ;
    }

    // for calculating the number of rows
    if (current_major_row < quark_field ){
      current_major_row = quark_field;
    }

    if ( element_number >= current_values_size ){
      current_values_size *= GROWTH_FACTOR;
      aux_coo_rows = (MKL_INT*) realloc(aux_coo_rows, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
      aux_coo_columns = (MKL_INT*) realloc(aux_coo_columns, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
      aux_coo_values = (float*) realloc(aux_coo_values, (current_values_size) * GROWTH_FACTOR * sizeof(float) );
    }

    /* normal coo property */
    aux_coo_values[element_number] = element_value;
    aux_coo_columns[element_number] = element_number;
    aux_coo_rows[element_number]=  quark_field - 1 ;

  }
  fclose(stream);

  array_pos++;
  MKL_INT end_quark = global_quark;
  (*quark_start_end)[array_pos] = end_quark;
  *quark_global_pos = array_pos;



  if (
      // in case it cant old the last element containing the NNZ
      ( (element_number+1) >= current_values_size )
      ||
      // in case the matrix aint squared and it cant old the last element containing the NNZ + the padding of 1
      ( (current_major_row < number_columns) && (element_number+2) >= current_values_size )
     ){
    current_values_size *= GROWTH_FACTOR;
    aux_coo_rows = (MKL_INT*) realloc(aux_coo_rows, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
    aux_coo_columns = (MKL_INT*) realloc(aux_coo_columns, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
    aux_coo_values = (float*) realloc(aux_coo_values, (current_values_size) * GROWTH_FACTOR * sizeof(float) );
  }

  if ( current_major_row < number_columns ){

    aux_coo_values[element_number] = 0.0;
    aux_coo_columns[element_number] = element_number;
    aux_coo_rows[element_number] = element_number;
    printf("\tpadding from (% x %d) to (%d x %d)\n", current_major_row, number_columns, element_number, element_number);
  }
  else {
    printf("no padding needed -- already squared (%d x %d)\n", element_number, element_number);
  }

  MKL_INT NNZ = element_number;
  number_rows = element_number;
  number_columns = element_number;

  /////////////////////////////////
  //   CONVERT FROM COO TO CSR
  /////////////////////////////////

  // if job[0]=2, the matrix in the coordinate format is converted to the CSR
  // format, and the column indices in CSR representation are sorted in the
  // increasing order within each row.
  job[0]= 2;

  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  job[1]= 0;

  // If job[2]=0, zero-based indexing for the matrix in coordinate format is used;
  job[2]= 0;

  job[3]= 0;
  // job[4]=nzmax - maximum number of the non-zero elements allowed if
  // job[0]=0.
  job[4]= NNZ;

  // If job[5]=0, all arrays acsr, ja, ia are filled in for the output storage.
  job[5]= 0;

  *A_csr_values = (float*) mkl_malloc ((NNZ * sizeof(float)), MEM_LINE_SIZE );
  *A_JA = (MKL_INT*) mkl_malloc (( NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *A_IA = (MKL_INT*) mkl_malloc (((number_rows+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  sparse_status_t status_coo_csr;
  mkl_scsrcoo (job, &number_rows, *A_csr_values, *A_JA, *A_IA, &NNZ, aux_coo_values, aux_coo_rows, aux_coo_columns, &status_coo_csr);
  *rows = number_rows;
  *columns = number_columns;
  *nnz = NNZ;

  free( aux_coo_values );
  free(  aux_coo_columns );
  free ( )aux_coo_rows );

  printf("readed matrix %d %d : NNZ %d\n\t\tproducing quarks between %d and %d\n", *rows, *columns, *nnz, initial_quark, end_quark );
}

void tbl_read_measure(
    char* table_name, MKL_INT tbl_column,
    MKL_INT* nnz, MKL_INT* rows, MKL_INT* columns,
    float** A_csr_values, MKL_INT** A_JA, MKL_INT** A_IA,
    MKL_INT **quark_start_end, MKL_INT* quark_global_pos
    ){

  MKL_INT current_values_size = ARRAY_SIZE;
  //define COO sparse-matrix M
  float* coo_values;
  MKL_INT* coo_rows;
  MKL_INT* coo_columns;

  coo_values = (float*) mkl_malloc (current_values_size * sizeof(float), MEM_LINE_SIZE );
  coo_rows = (MKL_INT*) mkl_malloc (current_values_size * sizeof(MKL_INT), MEM_LINE_SIZE );
  coo_columns = (MKL_INT*) mkl_malloc (current_values_size * sizeof(MKL_INT), MEM_LINE_SIZE );


  assert(coo_values != NULL);
  assert(coo_rows != NULL);
  assert(coo_columns != NULL);

  FILE* stream = fopen(table_name, "r");
  char line[1024];
  MKL_INT number_rows = - 1;
  MKL_INT number_columns = -1 ;
  MKL_INT element_number = 1;
  MKL_INT job[8];
  MKL_INT padding_quark = 0;

  // get to know how many quarks were used before
  MKL_INT array_pos = *quark_global_pos;
  MKL_INT initial_quark = *quark_start_end[array_pos];
  if (initial_quark > 1 ){
    padding_quark = initial_quark;
  }
  MKL_INT quark_field;
  MKL_INT global_quark;
  float element_value = 0.0;
  char *field = (char*) malloc( MAX_FIELD_SIZE * sizeof(char) );


  for( element_number = 0 ; (fgets(line, MAX_REG_SIZE, stream) ) ; ++element_number ){
    char* tmp_field = strdup(line);
    field = getfield(tmp_field, tbl_column, field);

    /* if arrays are full double its size */
    if ( element_number >= current_values_size ){
      current_values_size *= GROWTH_FACTOR;
      coo_values = (float*) mkl_realloc(coo_values, (current_values_size) * GROWTH_FACTOR * sizeof(float) );
      coo_rows = (MKL_INT*) mkl_realloc(coo_rows, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
      coo_columns = (MKL_INT*) mkl_realloc(coo_columns, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
    }

    coo_values[element_number] = atof(field);
    coo_columns[element_number] = element_number;
    coo_rows[element_number] = element_number;

    free(tmp_field);
  }

  fclose(stream);


  array_pos++;
  MKL_INT end_quark = global_quark;
  (*quark_start_end)[array_pos] = end_quark;
  *quark_global_pos = array_pos;



  MKL_INT NNZ = element_number;
  number_columns = element_number;
  number_rows = element_number;

  /////////////////////////////////
  //   CONVERT FROM COO TO CSR
  /////////////////////////////////

  // if job[0]=2, the matrix in the coordinate format is converted to the CSR
  // format, and the column indices in CSR representation are sorted in the
  // increasing order within each row.
  job[0]= 2;

  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  job[1]= 0;

  // If job[2]=0, zero-based indexing for the matrix in coordinate format is used;
  job[2]= 0;

  job[3]= 0;
  // job[4]=nzmax - maximum number of the non-zero elements allowed if
  // job[0]=0.
  job[4]= NNZ;

  // If job[5]=0, all arrays acsr, ja, ia are filled in for the output storage.
  job[5]= 0;

  *A_csr_values = (float*) mkl_malloc ((NNZ * sizeof(float)), MEM_LINE_SIZE );
  *A_JA = (MKL_INT*) mkl_malloc (( NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *A_IA = (MKL_INT*) mkl_malloc (((number_rows+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  sparse_status_t status_coo_csr;
  mkl_scsrcoo (job, &number_rows, *A_csr_values, *A_JA, *A_IA, &NNZ, coo_values, coo_rows, coo_columns, &status_coo_csr);
  mkl_free(coo_values);
  mkl_free(coo_rows);
  mkl_free(coo_columns);
  *rows = number_rows;
  *columns = number_columns;
  *nnz = NNZ;
}

void tbl_read_filter(
    char* table_name, MKL_INT tbl_column, int opp_code, char* comparation_key,
    MKL_INT* nnz, MKL_INT* rows, MKL_INT* columns,
    float** A_csr_values, MKL_INT** A_JA, MKL_INT** A_IA,
    MKL_INT **quark_start_end, MKL_INT* quark_global_pos
    ){

  MKL_INT current_values_size = ARRAY_SIZE;

  //define COO sparse-matrix M
  MKL_INT* aux_coo_rows;
  aux_coo_rows = (MKL_INT*) malloc (current_values_size * sizeof(MKL_INT));

  float* aux_coo_values;
  aux_coo_values = (float*) malloc (current_values_size * sizeof(float));

  FILE* stream = fopen(table_name, "r");
  char line[1024];
  MKL_INT number_rows = - 1;
  MKL_INT number_columns = -1;
  MKL_INT element_number = 1;
  MKL_INT job[8];
  MKL_INT padding_quark = 0;

  // read the input file
  for( element_number = 0 ; (fgets(line, MAX_REG_SIZE, stream) ) ; ++element_number ){

    char* field = (char*) malloc( MAX_FIELD_SIZE * sizeof(char) );
    char* tmp_field = strdup(line);
    field = getfield(tmp_field, tbl_column, field);

    MKL_INT quark_field;
    MKL_INT quark_zeroed = 0;

    MKL_INT returned_strcmp = strcmp( field , comparation_key );
    if (
        ( opp_code == LESS  && returned_strcmp >= 0 )
        ||
        ( opp_code == LESS_EQ  && returned_strcmp > 0 )
        ||
        ( opp_code == GREATER  && returned_strcmp <= 0 )
        ||
        ( opp_code == GREATER_EQ  && returned_strcmp < 0 )
       ){
      quark_zeroed = 1;
    }

    quark_field = g_quark_from_string (field);

    if (quark_field > 1 && element_number == 0 ){
      padding_quark = quark_field - 1;
    }
    quark_field -= padding_quark;
    /* if arrays are full double its size */
    if ( element_number >= current_values_size ){
      current_values_size *= GROWTH_FACTOR;
      aux_coo_rows = (MKL_INT*) realloc(aux_coo_rows, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
      aux_coo_values = (float*)  realloc(aux_coo_values, (current_values_size) * GROWTH_FACTOR * sizeof(float) );
    }

    if ( quark_zeroed == 1 ){
      aux_coo_values[element_number]=  0 ;
    }
    else {
      aux_coo_values[element_number]=  1 ;
    }

    /* normal coo property */
    aux_coo_rows[element_number]=  quark_field - 1 ;

    if (  quark_field > number_rows ) {
      number_rows =  quark_field;
    }
    free(tmp_field);
  }

  fclose(stream);
  MKL_INT NNZ = element_number;
  number_columns = element_number;

  //define COO sparse-matrix M
  float* coo_values;
  MKL_INT* coo_rows;
  MKL_INT* coo_columns;

  coo_values = (float*) mkl_malloc (((NNZ+1) * sizeof(float)), MEM_LINE_SIZE );
  coo_rows =  (MKL_INT*) mkl_malloc (((NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  coo_columns =  (MKL_INT*) mkl_malloc (((NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  for (int pos = 0; pos < NNZ; pos++) {
    coo_values[pos] = aux_coo_values[pos];
    coo_columns[pos] = pos;
    coo_rows[pos] = aux_coo_rows[pos];
  }

  coo_values[NNZ] = 0.0;
  coo_columns[NNZ] = NNZ;
  coo_rows[NNZ] = NNZ;
  number_rows = NNZ;
  NNZ++;

  // free(aux_coo_values);

  /////////////////////////////////
  //   CONVERT FROM COO TO CSR
  /////////////////////////////////

  // if job[0]=2, the matrix in the coordinate format is converted to the CSR
  // format, and the column indices in CSR representation are sorted in the
  // increasing order within each row.
  job[0]=  2;

  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  job[1]= 0;

  // If job[2]=0, zero-based indexing for the matrix in coordinate format is used;
  job[2]= 0;

  job[3]= 0;

  // job[4]=nzmax - maximum number of the non-zero elements allowed if job[0]=0.
  job[4]= NNZ;

  // If job[5]=0, all arrays acsr, ja, ia are filled in for the output storage.
  job[5]= 0;

  *A_csr_values = (float*) mkl_malloc ((NNZ * sizeof(float)), MEM_LINE_SIZE );
  *A_JA = (MKL_INT*) mkl_malloc (( NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *A_IA = (MKL_INT*) mkl_malloc (((number_rows+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  sparse_status_t status_coo_csr;
  mkl_scsrcoo (job, &number_rows, *A_csr_values, *A_JA, *A_IA, &NNZ, coo_values, coo_rows, coo_columns, &status_coo_csr);
  mkl_free(coo_values);
  mkl_free(coo_rows);
  mkl_free(coo_columns);
  *rows = number_rows;
  *columns = number_columns;
  *nnz = NNZ;
}

void tbl_read_filter_and(
    char* table_name, MKL_INT tbl_column, int opp_code, char* comparation_key, int opp_code2, char* comparation_key2,
    MKL_INT* nnz, MKL_INT* rows, MKL_INT* columns ,
    float** A_csr_values, MKL_INT** A_JA, MKL_INT** A_IA,
    MKL_INT **quark_start_end, MKL_INT* quark_global_pos
    ){

  MKL_INT current_values_size = ARRAY_SIZE;

  //define COO sparse-matrix M
  MKL_INT* aux_coo_rows;
  aux_coo_rows = (MKL_INT*) malloc (current_values_size * sizeof(MKL_INT));

  float* aux_coo_values;
  aux_coo_values = (float*) malloc (current_values_size * sizeof(float));

  FILE* stream = fopen(table_name, "r");
  char line[1024];
  MKL_INT number_rows = - 1;
  MKL_INT number_columns = -1;
  MKL_INT element_number = 1;
  MKL_INT job[8];
  MKL_INT padding_quark = 0;

  // read the input file
  for( element_number = 0 ; (fgets(line, MAX_REG_SIZE, stream) ) ; ++element_number ){

    char* field = (char*) malloc( MAX_FIELD_SIZE * sizeof(char) );
    char* tmp_field = strdup(line);
    field = getfield(tmp_field, tbl_column, field);

    MKL_INT quark_field;
    MKL_INT quark_zeroed = 0;

    MKL_INT returned_strcmp = strcmp( field , comparation_key );
    if (
        ( opp_code == LESS  && returned_strcmp >= 0 )
        ||
        ( opp_code == LESS_EQ  && returned_strcmp > 0 )
        ||
        ( opp_code == GREATER  && returned_strcmp <= 0 )
        ||
        ( opp_code == GREATER_EQ  && returned_strcmp < 0 )
       ){
      quark_zeroed = 1;
    }

    MKL_INT returned_strcmp2 = strcmp( field , comparation_key2 );
    if (
        ( opp_code2 == LESS  && returned_strcmp2 >= 0 )
        ||
        ( opp_code2 == LESS_EQ  && returned_strcmp2 > 0 )
        ||
        ( opp_code2 == GREATER  && returned_strcmp2 <= 0 )
        ||
        ( opp_code2 == GREATER_EQ  && returned_strcmp2 < 0 )
       ){
      quark_zeroed = 1;
    }

    quark_field = g_quark_from_string (field);

    if (quark_field > 1 && element_number == 0 ){
      padding_quark = quark_field - 1;
    }
    quark_field -= padding_quark;
    /* if arrays are full double its size */
    if ( element_number >= current_values_size ){
      current_values_size *= GROWTH_FACTOR;
      aux_coo_rows = (MKL_INT*) realloc(aux_coo_rows, (current_values_size) * GROWTH_FACTOR * sizeof(MKL_INT) );
      aux_coo_values = (float*)  realloc(aux_coo_values, (current_values_size) * GROWTH_FACTOR * sizeof(float) );
    }

    if ( quark_zeroed == 1 ){
      aux_coo_values[element_number]=  0 ;
    }
    else {
      aux_coo_values[element_number]=  1 ;
    }

    /* normal coo property */
    aux_coo_rows[element_number]=  quark_field - 1 ;

    if (  quark_field > number_rows ) {
      number_rows =  quark_field;
    }
    free(tmp_field);
  }

  fclose(stream);
  MKL_INT NNZ = element_number;
  number_columns = element_number;

  //define COO sparse-matrix M
  float* coo_values;
  MKL_INT* coo_rows;
  MKL_INT* coo_columns;

  coo_values = (float*) mkl_malloc (((NNZ+1) * sizeof(float)), MEM_LINE_SIZE );
  coo_rows =  (MKL_INT*) mkl_malloc (((NNZ+1)* sizeof(MKL_INT)), MEM_LINE_SIZE );
  coo_columns =  (MKL_INT*) mkl_malloc (((NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  for (int pos = 0; pos < NNZ; pos++) {
    coo_values[pos] = aux_coo_values[pos];
    coo_columns[pos] = pos;
    coo_rows[pos] = aux_coo_rows[pos];
  }

  coo_values[NNZ] = 0.0;
  coo_columns[NNZ] = NNZ;
  coo_rows[NNZ] = NNZ;
  number_rows = NNZ;
  NNZ++;

  // free(aux_coo_values);

  /////////////////////////////////
  //   CONVERT FROM COO TO CSR
  /////////////////////////////////

  // if job[0]=2, the matrix in the coordinate format is converted to the CSR
  // format, and the column indices in CSR representation are sorted in the
  // increasing order within each row.
  job[0]=  2;

  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  job[1]= 0;

  // If job[2]=0, zero-based indexing for the matrix in coordinate format is used;
  job[2]= 0;

  job[3]= 0;

  // job[4]=nzmax - maximum number of the non-zero elements allowed if job[0]=0.
  job[4]= NNZ;

  // If job[5]=0, all arrays acsr, ja, ia are filled in for the output storage.
  job[5]= 0;

  *A_csr_values = (float*) mkl_malloc ((NNZ * sizeof(float)), MEM_LINE_SIZE );
  *A_JA = (MKL_INT*) mkl_malloc (( NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *A_IA = (MKL_INT*) mkl_malloc (((number_rows+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  sparse_status_t status_coo_csr;
  mkl_scsrcoo (job, &number_rows, *A_csr_values, *A_JA, *A_IA, &NNZ, coo_values, coo_rows, coo_columns, &status_coo_csr);
  mkl_free(coo_values);
  mkl_free(coo_rows);
  mkl_free(coo_columns);
  *rows = number_rows;
  *columns = number_columns;
  *nnz = NNZ;
}

void csr_mx_selection_and(
    float* A_csr_values, MKL_INT* A_JA, MKL_INT* A_IA,
    MKL_INT A_NNZ, MKL_INT A_number_rows, MKL_INT A_number_columns,
    int opp_code, char* comparation_key, int opp_code2, char* comparation_key2,
    float** C_csr_values, MKL_INT** C_JA, MKL_INT** C_IA,
    MKL_INT* C_NNZ, MKL_INT* C_number_rows, MKL_INT* C_number_columns,
    MKL_INT **quark_start_end, MKL_INT* quark_global_pos
    ){

  *C_csr_values = (float*) mkl_malloc (((A_NNZ+1) * sizeof(float)), MEM_LINE_SIZE );
  *C_JA =  (MKL_INT*) mkl_malloc (((A_NNZ+1)* sizeof(MKL_INT)), MEM_LINE_SIZE );
  *C_IA =  (MKL_INT*) mkl_malloc (((A_NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  (*C_JA)[0:A_number_columns-1] = A_IA[0:A_number_columns-1];
  (*C_IA)[0:A_number_columns-1] = A_IA[0:A_number_columns-1];
  *C_NNZ = A_NNZ;
  *C_number_rows = A_number_rows;
  *C_number_columns = A_number_columns;

  char* field = (char*) malloc( MAX_FIELD_SIZE * sizeof(char) );

  // read the input file
  for( MKL_INT element_number = 1 ; element_number < A_number_columns ; ++element_number ){

    MKL_INT quark_zeroed = 0;

    field = (char*) g_quark_to_string ( element_number );
    if ( field != NULL ){
      MKL_INT returned_strcmp = strcmp( field , comparation_key );
      MKL_INT returned_strcmp2 = strcmp( field , comparation_key2 );

      if (
          ( opp_code == LESS  && returned_strcmp >= 0 )
          ||
          ( opp_code == LESS_EQ  && returned_strcmp > 0 )
          ||
          ( opp_code == GREATER  && returned_strcmp <= 0 )
          ||
          ( opp_code == GREATER_EQ  && returned_strcmp < 0 )
         ){
        quark_zeroed = 1;
      }
      if (
          ( opp_code2 == LESS  && returned_strcmp2 >= 0 )
          ||
          ( opp_code2 == LESS_EQ  && returned_strcmp2 > 0 )
          ||
          ( opp_code2 == GREATER  && returned_strcmp2 <= 0 )
          ||
          ( opp_code2 == GREATER_EQ  && returned_strcmp2 < 0 )
         ){
        quark_zeroed = 1;
      }
      if (quark_zeroed == 1 ){
        (*C_csr_values)[element_number] = 0;
      }
    }	
  }
}

void csr_mx_selection_or(
    float* A_csr_values, MKL_INT* A_JA, MKL_INT* A_IA,
    MKL_INT A_NNZ, MKL_INT A_number_rows, MKL_INT A_number_columns,
    int opp_code, char* comparation_key, int opp_code2, char* comparation_key2,
    float** C_csr_values, MKL_INT** C_JA, MKL_INT** C_IA,
    MKL_INT* C_NNZ, MKL_INT* C_number_rows, MKL_INT* C_number_columns,
    MKL_INT **quark_start_end, MKL_INT* quark_global_pos
    ){

  *C_csr_values = (float*) mkl_malloc (((A_NNZ+1) * sizeof(float)), MEM_LINE_SIZE );
  *C_JA =  (MKL_INT*) mkl_malloc (((A_NNZ+1)* sizeof(MKL_INT)), MEM_LINE_SIZE );
  *C_IA =  (MKL_INT*) mkl_malloc (((A_NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  *C_IA[0:A_NNZ] = A_IA[0:A_NNZ];
  *C_JA[0:A_NNZ] = A_JA[0:A_NNZ];
  *C_NNZ = A_NNZ;
  *C_number_rows = A_number_rows;
  *C_number_columns = A_number_columns;

  char* field = (char*) malloc( MAX_FIELD_SIZE * sizeof(char) );

  // read the input file
  for( MKL_INT element_number = 0 ; element_number < A_NNZ ; ++element_number ){

    MKL_INT quark_zeroed = 0;

    field = (char*) g_quark_to_string ( element_number );

    MKL_INT returned_strcmp = strcmp( field , comparation_key );
    MKL_INT returned_strcmp2 = strcmp( field , comparation_key2 );

    if (
        ( opp_code == LESS  && returned_strcmp >= 0 )
        ||
        ( opp_code == LESS_EQ  && returned_strcmp > 0 )
        ||
        ( opp_code == GREATER  && returned_strcmp <= 0 )
        ||
        ( opp_code == GREATER_EQ  && returned_strcmp < 0 )
       ){
      quark_zeroed = 1;
    }
    else if (
        ( opp_code2 == LESS  && returned_strcmp2 >= 0 )
        ||
        ( opp_code2 == LESS_EQ  && returned_strcmp2 > 0 )
        ||
        ( opp_code2 == GREATER  && returned_strcmp2 <= 0 )
        ||
        ( opp_code2 == GREATER_EQ  && returned_strcmp2 < 0 )
        ){
      quark_zeroed = 1;
    }
    if (quark_zeroed == 1 ){
      *C_csr_values[element_number] = 0;
    }
  }

}


void csr_mx_selection(
    float* A_csr_values, MKL_INT* A_JA, MKL_INT* A_IA,
    MKL_INT A_NNZ, MKL_INT A_number_rows, MKL_INT A_number_columns,
    int opp_code, char* comparation_key,
    float** C_csr_values, MKL_INT** C_JA, MKL_INT** C_IA,
    MKL_INT* C_NNZ, MKL_INT* C_number_rows, MKL_INT* C_number_columns,
    MKL_INT **quark_start_end, MKL_INT* quark_global_pos
    ){


  *C_csr_values = (float*) mkl_malloc (((A_NNZ+1) * sizeof(float)), MEM_LINE_SIZE );
  *C_JA =  (MKL_INT*) mkl_malloc (((A_NNZ+1)* sizeof(MKL_INT)), MEM_LINE_SIZE );
  *C_IA =  (MKL_INT*) mkl_malloc (((A_NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  *C_IA[0:A_NNZ] = A_IA[0:A_NNZ];
  *C_JA[0:A_NNZ] = A_JA[0:A_NNZ];
  *C_NNZ = A_NNZ;
  *C_number_rows = A_number_rows;
  *C_number_columns = A_number_columns;

  char* field = (char*) malloc( MAX_FIELD_SIZE * sizeof(char) );

  // read the input file
  for( MKL_INT element_number = 0 ; element_number < A_NNZ ; ++element_number ){

    MKL_INT quark_zeroed = 0;

    field = (char*) g_quark_to_string ( element_number );

    MKL_INT returned_strcmp = strcmp( field , comparation_key );

    if (
        ( opp_code == LESS  && returned_strcmp >= 0 )
        ||
        ( opp_code == LESS_EQ  && returned_strcmp > 0 )
        ||
        ( opp_code == GREATER  && returned_strcmp <= 0 )
        ||
        ( opp_code == GREATER_EQ  && returned_strcmp < 0 )
       ){
      quark_zeroed = 1;
    }

    if (quark_zeroed == 1 ){
      *C_csr_values[element_number] = 0;
    }
  }

}


void check_errors( sparse_status_t stat ){
  if ( stat == SPARSE_STATUS_SUCCESS ){
    printf( "SPARSE_STATUS_SUCCESS.\n");
  }
  if ( stat == SPARSE_STATUS_NOT_INITIALIZED ){
    printf( "SPARSE_STATUS_NOT_INITIALIZED.\n");
  }
  if ( stat == SPARSE_STATUS_ALLOC_FAILED ){
    printf( "SPARSE_STATUS_ALLOC_FAILED.\n");
  }
  if ( stat == SPARSE_STATUS_INVALID_VALUE ){
    printf( "SPARSE_STATUS_INVALID_VALUE.\n");
  }
  if ( stat == SPARSE_STATUS_EXECUTION_FAILED){
    printf( "SPARSE_STATUS_EXECUTION_FAILED.\n");
  }
  if ( stat == SPARSE_STATUS_INTERNAL_ERROR){
    printf( "SPARSE_STATUS_INTERNAL_ERROR.\n");
  }
  if ( stat == SPARSE_STATUS_NOT_SUPPORTED){
    printf( "SPARSE_STATUS_NOT_SUPPORTED.\n");
  }
}

///////////////////////////////////// OPS /////////////////////////////////////
///////////////////////////////////// OPS /////////////////////////////////////
///////////////////////////////////// OPS /////////////////////////////////////

/////////////////////////////////
//
//   COMPUTE HADAMARD
//
/////////////////////////////////
void csr_hadamard(
    float *restrict A_csr_values, MKL_INT *restrict A_JA, MKL_INT *restrict A_IA,
    MKL_INT A_NNZ, MKL_INT A_number_rows, MKL_INT A_number_columns,
    float *restrict B_csr_values, MKL_INT *restrict B_JA, MKL_INT *restrict B_IA,
    MKL_INT B_NNZ, MKL_INT B_number_rows, MKL_INT B_number_columns,
    float **restrict C_csr_values, MKL_INT **restrict C_JA, MKL_INT **restrict C_IA,
    MKL_INT* C_NNZ, MKL_INT* C_number_rows, MKL_INT* C_number_columns
    ){

  MKL_INT job[8];

  /////////////////////////////////////
  // PREPARE FOR OPERATION
  /////////////////////////////////////
  //////////////////////////////////////////
  ///////   CONVERT A and B from CSR to CSC
  //////////////////////////////////////////

  // If job[0]=0, the matrix in the CSR format is converted to the CSC format;
  job[0] = 0;

  // job[1]
  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  // if job[1]=1, one-based indexing for the matrix in CSR format is used.
  job[1] = 0;

  // job[2]
  // If job[2]=0, zero-based indexing for the matrix in the CSC format is used;
  // if job[2]=1, one-based indexing for the matrix in the CSC format is used.
  job[2] = 0;

  // job[5] - job indicator.
  // If job[5]=0, only arrays ja1, ia1 are filled in for the output storage.
  // If job[5]≠0, all output arrays acsc, ja1, and ia1 are filled in for the output storage.
  job[5] = 1;
  sparse_status_t status_convert_csc;
  __declspec(align(MEM_LINE_SIZE))	float* A_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* A_JA1;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* A_IA1;

  A_csc_values = (float*) mkl_malloc (( A_NNZ * sizeof(float) ), MEM_LINE_SIZE );
  A_JA1 = (MKL_INT*) mkl_malloc (( A_NNZ * sizeof(MKL_INT) ), MEM_LINE_SIZE );
  A_IA1 = (MKL_INT*) mkl_malloc (((A_NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &A_NNZ, A_csr_values, A_JA, A_IA, A_csc_values, A_JA1, A_IA1, &status_convert_csc);

  __declspec(align(MEM_LINE_SIZE))	float* B_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* B_JA1;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* B_IA1;

  B_csc_values = (float*) mkl_malloc (( B_NNZ * sizeof(float)), MEM_LINE_SIZE );
  B_JA1 = (MKL_INT*) mkl_malloc (( B_NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  B_IA1 = (MKL_INT*) mkl_malloc (( (B_NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &B_NNZ, B_csr_values, B_JA, B_IA, B_csc_values, B_JA1, B_IA1, &status_convert_csc);

  /////////////////////////////////
  //   COMPUTE HADAMARD
  /////////////////////////////////

  __declspec(align(MEM_LINE_SIZE))	float* C_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* C_JA1;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* C_IA1;

  C_csc_values = (float*) mkl_malloc (( A_NNZ * sizeof(float)), MEM_LINE_SIZE );
  C_JA1 = (MKL_INT*) mkl_malloc (( A_NNZ  * sizeof(MKL_INT)), MEM_LINE_SIZE );
  C_IA1 = (MKL_INT*) mkl_malloc (( (A_number_columns+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  __declspec(align(MEM_LINE_SIZE))	MKL_INT at_column = 0;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT end_column = A_number_columns;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT scalar_B = B_number_rows;

  __assume_aligned(C_IA1, MEM_LINE_SIZE);
  __assume_aligned(A_IA1, MEM_LINE_SIZE);
  __assume_aligned(C_csc_values, MEM_LINE_SIZE);
  __assume_aligned(B_csc_values, MEM_LINE_SIZE);
  __assume_aligned(A_csc_values, MEM_LINE_SIZE);
  __assume_aligned(B_JA1, MEM_LINE_SIZE);
  __assume_aligned(A_JA1, MEM_LINE_SIZE);
  __assume_aligned(C_JA1, MEM_LINE_SIZE);

  for ( MKL_INT at_column = 0; at_column < end_column; ++at_column){
    // insert start of column int C_IA1
    MKL_INT iaa = A_IA1[at_column];
    MKL_INT iab = B_IA1[at_column];

    float c_value = B_csc_values[at_column];
    c_value *= A_csc_values[at_column];
    C_JA1[at_column] = at_column;
    C_IA1[at_column] = iaa;

    if (iaa != iab ){
      c_value = 0;
    }
    C_csc_values[at_column] = c_value;
  }

  C_IA1[A_number_columns] = A_NNZ;

  /////////////////////////////////
  //   CONVERT C from CSC to CSR
  ////////////////////////////////

  // If job[0]=1, the matrix in the CSC format is converted to the CSR format.
  job[0] = 1;

  // job[1]
  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  // if job[1]=1, one-based indexing for the matrix in CSR format is used.
  job[1] = 0;

  // job[2]
  // If job[2]=0, zero-based indexing for the matrix in the CSC format is used;
  // if job[2]=1, one-based indexing for the matrix in the CSC format is used.
  job[2] = 0;

  // job[5] - job indicator.
  // If job[5]=0, only arrays ja1, ia1 are filled in for the output storage.
  // If job[5]≠0, all output arrays acsc, ja1, and ia1 are filled in for the output storage.
  job[5] = 1;
  sparse_status_t status_convert_csr;

  *C_csr_values = (float*) mkl_malloc (( A_NNZ * sizeof(float)), MEM_LINE_SIZE );
  *C_JA = (MKL_INT*) mkl_malloc (( A_NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *C_IA = (MKL_INT*) mkl_malloc (( (A_number_rows + 1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &A_NNZ, *C_csr_values, *C_JA, *C_IA, C_csc_values, C_JA1, C_IA1, &status_convert_csr);

  *C_number_rows = A_number_rows ;
  *C_number_columns = A_number_columns;
  *C_NNZ = A_NNZ;
}

/////////////////////////////////
//
//   COMPUTE KHATRI-RAO
//
/////////////////////////////////
void csr_csr_krao(
    float *restrict A_csr_values, MKL_INT *restrict A_JA, MKL_INT *restrict A_IA,
    MKL_INT A_NNZ, MKL_INT A_number_rows, MKL_INT A_number_columns,
    float *restrict B_csr_values, MKL_INT *restrict B_JA, MKL_INT *restrict B_IA ,
    MKL_INT B_NNZ, MKL_INT B_number_rows, MKL_INT B_number_columns,
    float **restrict C_csr_values, MKL_INT **restrict C_JA, MKL_INT **restrict C_IA,
    MKL_INT* C_NNZ, MKL_INT* C_number_rows, MKL_INT* C_number_columns
    ){

  /////////////////////////////////////
  // PREPARE FOR OPERATION
  /////////////////////////////////////

  //////////////////////////////////////////
  ///////   CONVERT A and B from CSR to CSC
  //////////////////////////////////////////

  MKL_INT job[8];
  // If job[0]=0, the matrix in the CSR format is converted to the CSC format;
  job[0] = 0;

  // job[1]
  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  // if job[1]=1, one-based indexing for the matrix in CSR format is used.
  job[1] = 0;

  // job[2]
  // If job[2]=0, zero-based indexing for the matrix in the CSC format is used;
  // if job[2]=1, one-based indexing for the matrix in the CSC format is used.
  job[2] = 0;

  // job[5] - job indicator.
  // If job[5]=0, only arrays ja1, ia1 are filled in for the output storage.
  // If job[5]≠0, all output arrays acsc, ja1, and ia1 are filled in for the output storage.
  job[5] = 1;
  sparse_status_t status_convert_csc;

  /////////////////////////////////
  //   DECLARE MATRICES
  /////////////////////////////////

  __declspec(align(MEM_LINE_SIZE))	float* A_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* A_JA1;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* A_IA1;

  __declspec(align(MEM_LINE_SIZE))	float* B_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* B_JA1;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* B_IA1;

  __declspec(align(MEM_LINE_SIZE))	float* C_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE)) 	MKL_INT* C_JA1;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* C_IA1;

  /////////////////////////////////
  //   ALLOCATE MEMORY
  /////////////////////////////////

  A_csc_values = (float*) mkl_malloc (( A_NNZ * sizeof(float) ), MEM_LINE_SIZE );
  A_JA1 = (MKL_INT*) mkl_malloc (( A_NNZ * sizeof(MKL_INT) ), MEM_LINE_SIZE );
  A_IA1 = (MKL_INT*) mkl_malloc (((A_NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &A_NNZ, A_csr_values, A_JA, A_IA, A_csc_values, A_JA1, A_IA1, &status_convert_csc);

  B_csc_values = (float*) mkl_malloc (( B_NNZ * sizeof(float)), MEM_LINE_SIZE );
  B_JA1 = (MKL_INT*) mkl_malloc (( B_NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  B_IA1 = (MKL_INT*) mkl_malloc (( (B_NNZ+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &B_NNZ, B_csr_values, B_JA, B_IA, B_csc_values, B_JA1, B_IA1, &status_convert_csc);

  C_csc_values = (float*) mkl_malloc (( A_NNZ * sizeof(float)), MEM_LINE_SIZE );
  C_JA1 = (MKL_INT*) mkl_malloc (( A_NNZ  * sizeof(MKL_INT)), MEM_LINE_SIZE );
  C_IA1 = (MKL_INT*) mkl_malloc (( (A_number_columns+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  /////////////////////////////////
  //   COMPUTE KRAO
  /////////////////////////////////

  __declspec(align(MEM_LINE_SIZE))	MKL_INT end_column = A_number_columns;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT scalar_B = B_number_rows;

  // n=16 for SSE, n=32 for AV
  __assume_aligned(C_IA1, MEM_LINE_SIZE);
  __assume_aligned(A_IA1, MEM_LINE_SIZE);
  __assume_aligned(C_csc_values, MEM_LINE_SIZE);
  __assume_aligned(B_csc_values, MEM_LINE_SIZE);
  __assume_aligned(A_csc_values, MEM_LINE_SIZE);
  __assume_aligned(B_JA1, MEM_LINE_SIZE);
  __assume_aligned(A_JA1, MEM_LINE_SIZE);
  __assume_aligned(C_JA1, MEM_LINE_SIZE);

  /////////////////////////////////
  //   COMPUTE KRAO
  /////////////////////////////////

  C_IA1[0:end_column] = A_IA1[0:end_column];
  C_IA1[A_number_columns] = A_NNZ;
  C_csc_values[0:end_column] =  B_csc_values[0:end_column] *  A_csc_values[0:end_column];
  C_JA1[0:end_column] = B_JA1[0:end_column] + ( A_JA1[0:end_column] * scalar_B );

  /////////////////////////////////
  //   CONVERT C from CSC to CSR
  ////////////////////////////////

  // If job[0]=1, the matrix in the CSC format is converted to the CSR format.
  job[0] = 1;

  // job[1]
  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  // if job[1]=1, one-based indexing for the matrix in CSR format is used.
  job[1] = 0;

  // job[2]
  // If job[2]=0, zero-based indexing for the matrix in the CSC format is used;
  // if job[2]=1, one-based indexing for the matrix in the CSC format is used.
  job[2] = 0;

  // job[5] - job indicator.
  // If job[5]=0, only arrays ja1, ia1 are filled in for the output storage.
  // If job[5]≠0, all output arrays acsc, ja1, and ia1 are filled in for the output storage.
  job[5] = 1;
  sparse_status_t status_convert_csr;
  MKL_INT final_number_rows = A_number_rows + B_number_rows;

  /////////////////////////////////
  //   ALLOCATE MEMORY
  /////////////////////////////////

  *C_csr_values = (float*) mkl_malloc (( A_NNZ * sizeof(float)), MEM_LINE_SIZE );
  *C_JA = (MKL_INT*) mkl_malloc (( A_NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *C_IA = (MKL_INT*) mkl_malloc (( ( final_number_rows + 1 ) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &A_NNZ, *C_csr_values, *C_JA, *C_IA, C_csc_values, C_JA1, C_IA1, &status_convert_csr);

  *C_number_rows = final_number_rows;
  *C_number_columns = A_number_columns;
  *C_NNZ = A_NNZ;
}

/////////////////////////////////
//
//   COMPUTE KHATRI-RAO
//
/////////////////////////////////
void csc_csr_krao(
    float *restrict A_csc_values, MKL_INT *restrict A_JA1, MKL_INT *restrict A_IA1,
    MKL_INT A_NNZ, MKL_INT A_number_rows, MKL_INT A_number_columns,
    float *restrict B_csc_values, MKL_INT *restrict B_JA1, MKL_INT *restrict B_IA1 ,
    MKL_INT B_NNZ, MKL_INT B_number_rows, MKL_INT B_number_columns,
    float **restrict C_csr_values, MKL_INT **restrict C_JA, MKL_INT **restrict C_IA,
    MKL_INT* C_NNZ, MKL_INT* C_number_rows, MKL_INT* C_number_columns
    ){

  __declspec(align(MEM_LINE_SIZE))	float* C_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE)) 	MKL_INT* C_JA1;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT* C_IA1;

  /////////////////////////////////
  //   ALLOCATE MEMORY
  /////////////////////////////////

  C_csc_values = (float*) mkl_malloc (( A_NNZ * sizeof(float)), MEM_LINE_SIZE );
  C_JA1 = (MKL_INT*) mkl_malloc (( A_NNZ  * sizeof(MKL_INT)), MEM_LINE_SIZE );
  C_IA1 = (MKL_INT*) mkl_malloc (( (A_number_columns+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  /////////////////////////////////
  //   COMPUTE KRAO
  /////////////////////////////////

  __declspec(align(MEM_LINE_SIZE))	MKL_INT end_column = A_number_columns;
  __declspec(align(MEM_LINE_SIZE))	MKL_INT scalar_B = B_number_rows;

  // n=16 for SSE, n=32 for AV
  __assume_aligned(C_IA1, MEM_LINE_SIZE);
  __assume_aligned(A_IA1, MEM_LINE_SIZE);
  __assume_aligned(C_csc_values, MEM_LINE_SIZE);
  __assume_aligned(B_csc_values, MEM_LINE_SIZE);
  __assume_aligned(A_csc_values, MEM_LINE_SIZE);
  __assume_aligned(B_JA1, MEM_LINE_SIZE);
  __assume_aligned(A_JA1, MEM_LINE_SIZE);
  __assume_aligned(C_JA1, MEM_LINE_SIZE);

  /////////////////////////////////
  //   COMPUTE KRAO
  /////////////////////////////////
  //  __declspec(align(MEM_LINE_SIZE))	MKL_INT nthreads = omp_get_num_threads();
  // __declspec(align(MEM_LINE_SIZE))	MKL_INT columns_per_worker =     A_number_columns / nthreads;

  for ( MKL_INT at_column = 0 ; at_column < A_number_columns ; ++at_column ){
    C_IA1[at_column] = A_IA1[at_column];
  }

  for ( MKL_INT at_column = 0 ; at_column < A_number_columns ; ++at_column ){
    C_csc_values[at_column] =  B_csc_values[at_column] *  A_csc_values[at_column];
  }

  for ( MKL_INT at_column = 0 ; at_column < A_number_columns ; ++at_column ){
    C_JA1[at_column] = B_JA1[at_column] + ( A_JA1[at_column] * scalar_B );
  }
  C_IA1[A_number_columns] = A_NNZ;

  /////////////////////////////////
  //   CONVERT C from CSC to CSR
  ////////////////////////////////
  sparse_status_t status_convert_csc;

  MKL_INT job[8];

  // If job[0]=1, the matrix in the CSC format is converted to the CSR format.
  job[0] = 1;

  // job[1]
  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  // if job[1]=1, one-based indexing for the matrix in CSR format is used.
  job[1] = 0;

  // job[2]
  // If job[2]=0, zero-based indexing for the matrix in the CSC format is used;
  // if job[2]=1, one-based indexing for the matrix in the CSC format is used.
  job[2] = 0;

  // job[5] - job indicator.
  // If job[5]=0, only arrays ja1, ia1 are filled in for the output storage.
  // If job[5]≠0, all output arrays acsc, ja1, and ia1 are filled in for the output storage.
  job[5] = 1;
  sparse_status_t status_convert_csr;
  MKL_INT final_number_rows = A_number_rows + B_number_rows;

  /////////////////////////////////
  //   ALLOCATE MEMORY
  /////////////////////////////////

  *C_csr_values = (float*) mkl_malloc (( A_NNZ * sizeof(float)), MEM_LINE_SIZE );
  *C_JA = (MKL_INT*) mkl_malloc (( A_NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *C_IA = (MKL_INT*) mkl_malloc (( ( final_number_rows + 1 ) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &A_NNZ, *C_csr_values, *C_JA, *C_IA, C_csc_values, C_JA1, C_IA1, &status_convert_csr);

  *C_number_rows = final_number_rows;
  *C_number_columns = A_number_columns;
  *C_NNZ = A_NNZ;
}

/////////////////////////////////
//
//   COMPUTE KRONECKER PRODUCT
//
/////////////////////////////////

void csr_kron(
    float *restrict A_csr_values, MKL_INT *restrict A_JA, MKL_INT *restrict A_IA, MKL_INT A_NNZ, MKL_INT A_number_rows, MKL_INT A_number_columns,
    float *restrict B_csr_values, MKL_INT *restrict B_JA, MKL_INT *restrict B_IA , MKL_INT B_NNZ, MKL_INT B_number_rows, MKL_INT B_number_columns,
    float**restrict C_csr_values, MKL_INT**restrict C_JA, MKL_INT**restrict C_IA, MKL_INT* C_NNZ, MKL_INT* C_number_rows, MKL_INT* C_number_columns
    ){

  /////////////////////////////////////
  // PREPARE FOR OPERATION
  /////////////////////////////////////
  //////////////////////////////////////////
  ///////   CONVERT A and B from CSR to CSC
  //////////////////////////////////////////

  MKL_INT job[8];
  // If job[0]=0, the matrix in the CSR format is converted to the CSC format;
  job[0] = 0;
  // job[1]
  job[1] = 0;
  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  // if job[1]=1, one-based indexing for the matrix in CSR format is used.
  // job[2]
  // If job[2]=0, zero-based indexing for the matrix in the CSC format is used;
  // if job[2]=1, one-based indexing for the matrix in the CSC format is used.
  job[2] = 0;
  // job[5] - job indicator.
  // If job[5]=0, only arrays ja1, ia1 are filled in for the output storage.
  // If job[5]≠0, all output arrays acsc, ja1, and ia1 are filled in for the output storage.
  job[5] = 1;
  sparse_status_t status_convert_csc;
  float* A_csc_values = NULL;
  MKL_INT* A_JA1;
  MKL_INT* A_IA1;

  A_csc_values = (float*) mkl_malloc (( A_NNZ * sizeof(float) ), MEM_LINE_SIZE );
  A_JA1 = (MKL_INT*) mkl_malloc (( A_NNZ * sizeof(MKL_INT) ), MEM_LINE_SIZE );
  A_IA1 = (MKL_INT*) mkl_malloc (( (A_number_columns+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &A_NNZ, A_csr_values, A_JA, A_IA, A_csc_values, A_JA1, A_IA1, &status_convert_csc);

  float* B_csc_values = NULL;
  MKL_INT* B_JA1;
  MKL_INT* B_IA1;

  B_csc_values = (float*) mkl_malloc (( B_NNZ * sizeof(float)), MEM_LINE_SIZE );
  B_JA1 = (MKL_INT*) mkl_malloc (( B_NNZ * sizeof(MKL_INT)), MEM_LINE_SIZE );
  B_IA1 = (MKL_INT*) mkl_malloc (( (B_number_columns+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  mkl_scsrcsc(job, &B_NNZ, B_csr_values, B_JA, B_IA, B_csc_values, B_JA1, B_IA1, &status_convert_csc);


  /////////////////////////////////
  //   COMPUTE KRON
  /////////////////////////////////
  float* C_csc_values = NULL;
  MKL_INT* C_JA1;
  MKL_INT* C_IA1;
  MKL_INT C_nnz = A_NNZ * B_NNZ;
  MKL_INT C_ncols = A_number_columns * B_number_columns;
  MKL_INT C_nrows = A_number_rows * B_number_rows;
  C_csc_values = (float*) mkl_malloc (( (C_nnz) * sizeof(float)), MEM_LINE_SIZE );
  C_JA1 = (MKL_INT*) mkl_malloc (( (C_nnz) * sizeof(MKL_INT)), MEM_LINE_SIZE );
  C_IA1 = (MKL_INT*) mkl_malloc (( (C_ncols +1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  MKL_INT at_column = 0;
  MKL_INT end_column = 0;
  MKL_INT row_pos = 0;

  MKL_INT end_column_A = A_number_columns;
  MKL_INT end_column_B = B_number_columns;
  MKL_INT scalar_B = B_number_rows;
  float value;
  // n=16 for SSE, n=32 for AV
  __assume_aligned(C_IA1, MEM_LINE_SIZE);
  __assume_aligned(A_IA1, MEM_LINE_SIZE);
  __assume_aligned(C_csc_values, MEM_LINE_SIZE);
  __assume_aligned(B_csc_values, MEM_LINE_SIZE);
  __assume_aligned(A_csc_values, MEM_LINE_SIZE);
  __assume_aligned(B_JA1, MEM_LINE_SIZE);
  __assume_aligned(A_JA1, MEM_LINE_SIZE);
  __assume_aligned(C_JA1, MEM_LINE_SIZE);

#pragma vector always aligned
#pragma ivdep
  for ( MKL_INT at_column_A = 0, at_column = 0 ; at_column_A < end_column_A; ++at_column_A, ++at_column ){
    end_column = at_column + B_number_columns;
    C_IA1[at_column:end_column] = ( A_IA1[at_column_A] * scalar_B ) + B_IA1[at_column:end_column];
  }
  C_IA1[at_column] = C_nnz;

#pragma vector always aligned
#pragma ivdep
  for ( MKL_INT at_column_A = 0, at_column = 0 ; at_column_A < end_column_A; ++at_column_A, ++at_column ){
    end_column = at_column + B_number_columns;
    C_csc_values[at_column:end_column] =  B_csc_values[at_column:end_column] *  A_csc_values[at_column:end_column];
  }

#pragma vector always aligned
#pragma ivdep
  for ( MKL_INT at_column_A = 0, at_column = 0 ; at_column_A < end_column_A; ++at_column_A, ++at_column ){
    end_column = at_column + B_number_columns;
    C_JA1[at_column:end_column] = B_JA1[at_column:end_column] + ( A_JA1[at_column:end_column] * scalar_B );
  }

  /////////////////////////////////
  //   CONVERT C from CSC to CSR
  /////////////////////////////////

  // If job[0]=1, the matrix in the CSC format is converted to the CSR format.
  job[0] = 1;

  // job[1]
  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  // if job[1]=1, one-based indexing for the matrix in CSR format is used.
  job[1] = 0;

  // job[2]
  // If job[2]=0, zero-based indexing for the matrix in the CSC format is used;
  // if job[2]=1, one-based indexing for the matrix in the CSC format is used.
  job[2] = 0;

  // job[5] - job indicator.
  // If job[5]=0, only arrays ja1, ia1 are filled in for the output storage.
  // If job[5]≠0,  all output arrays acsr, ja, and ia are filled in for the output storage. 
  job[5] = 1;

  sparse_status_t status_convert_csr;
  *C_csr_values = (float*) mkl_malloc (( C_nnz * sizeof(float)), MEM_LINE_SIZE );
  *C_JA = (MKL_INT*) mkl_malloc (( C_nnz * sizeof(MKL_INT)), MEM_LINE_SIZE );
  *C_IA = (MKL_INT*) mkl_malloc (( (C_nnz + 1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  mkl_scsrcsc(job, &C_nnz, *C_csr_values, *C_JA, *C_IA, C_csc_values, C_JA1, C_IA1, &status_convert_csr);

  *C_number_rows = C_nrows ;
  *C_number_columns = C_ncols;
  *C_NNZ = C_nnz;
}

#endif
