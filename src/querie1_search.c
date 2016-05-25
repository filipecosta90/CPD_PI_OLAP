/* ---------------------------------------------------------------------------
 **    Filename: querie1_search.c
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
 **              It encodes TPC-H querie-1 into Linear Algebra Operations.
 **
 **
 **     Authors: Filipe Oliveira <a57816@alunos.uminho.pt>
 **          and Sérgio Caldas   <a57779@alunos.uminho.pt>
 **
 ** University of Minho, High Performance Computing Dpt. , May 2016
 ** -------------------------------------------------------------------------*/

#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mkl_spblas.h"
#include "mkl_types.h"
#include "mkl.h"
#include "olap_search.h"
#include "timer.h"

double global_time_start, global_time_stop, total_time;

void writeResults ( char* dataset ) {
  total_time = global_time_stop - global_time_start;
  char file_write[80];
  strcpy(file_write, "timing/timings_no_vec_");
  strcat(file_write, dataset);
  strcat(file_write, ".dat");

  FILE* stream = fopen(file_write, "a+");
  fprintf(stream, "%s,%f\n",dataset, total_time);
  fclose(stream);
}

int main( int argc, char* argv[]){

  char return_flag[80];
  char line_status[80];
  char quantity[80];
  char shipdate_gt[80];
  char shipdate_lt[80];
  char working_dir[80];

  strcpy(working_dir, "__quark_mx_" );
  strcat(working_dir, argv[1]);

  strcat(return_flag, working_dir);
  strcat(return_flag, "/return_flag_");
  strcat(return_flag, argv[1]);
  strcat(return_flag, ".mx");

  strcat(line_status, working_dir);
  strcat(line_status, "/line_status_");
  strcat(line_status, argv[1]);
  strcat(line_status, ".mx");

  strcat(quantity, working_dir);
  strcat(quantity, "/quantity_");
  strcat(quantity, argv[1]);
  strcat(quantity, ".mx");

  strcat(shipdate_gt, working_dir);
  strcat(shipdate_gt, "/shipdate_gt_");
  strcat(shipdate_gt, argv[1]);
  strcat(shipdate_gt, ".mx");

  strcat(shipdate_lt, working_dir);
  strcat(shipdate_lt, "/shipdate_lt_");
  strcat(shipdate_lt, argv[1]);
  strcat(shipdate_lt, ".mx");

  //////////////////////////////////////////
  //        CONVERT from CSR to CSC
  //////////////////////////////////////////

  MKL_INT job_csr_csc[8];
  // If job[0]=0, the matrix in the CSR format is converted to the CSC format;
  job_csr_csc[0] = 0;

  // job[1]
  // If job[1]=0, zero-based indexing for the matrix in CSR format is used;
  // if job[1]=1, one-based indexing for the matrix in CSR format is used.
  job_csr_csc[1] = 0;

  // job[2]
  // If job[2]=0, zero-based indexing for the matrix in the CSC format is used;
  // if job[2]=1, one-based indexing for the matrix in the CSC format is used.
  job_csr_csc[2] = 0;

  // job[5] - job indicator.
  // If job[5]=0, only arrays ja1, ia1 are filled in for the output storage.
  // If job[5]≠0, all output arrays acsc, ja1, and ia1 are filled in for the output storage.
  job_csr_csc[5] = 1;
  sparse_status_t status_convert_to_csc;

  /** ---------------------------------------------------------------------------
   ** Return Flag Matrix
   ** -------------------------------------------------------------------------*/
  //CSR
  __declspec(align(MEM_LINE_SIZE))  float* return_flag_csr_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* return_flag_JA;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* return_flag_IA;
  //CSC
  __declspec(align(MEM_LINE_SIZE))  float* return_flag_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* return_flag_JA_csc;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* return_flag_IA_csc;
  //COMMON
  MKL_INT return_flag_rows;
  MKL_INT return_flag_columns;
  MKL_INT return_flag_nnz;

  /** ---------------------------------------------------------------------------
   ** Line Status Matrix
   ** -------------------------------------------------------------------------*/
  //CSR
  __declspec(align(MEM_LINE_SIZE))  float* line_status_csr_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* line_status_JA;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* line_status_IA;
  //CSC
  __declspec(align(MEM_LINE_SIZE))  float* line_status_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* line_status_JA_csc;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* line_status_IA_csc;
  //COMMON
  MKL_INT line_status_rows;
  MKL_INT line_status_columns;
  MKL_INT line_status_nnz;

  /** ---------------------------------------------------------------------------
   ** Quantity Matrix
   ** -------------------------------------------------------------------------*/
  //CSR
  __declspec(align(MEM_LINE_SIZE))  float* quantity_csr_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* quantity_JA;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* quantity_IA;
  //CSC
  __declspec(align(MEM_LINE_SIZE))  float* quantity_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* quantity_JA_csc;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* quantity_IA_csc;
  //COMMON
  MKL_INT quantity_rows;
  MKL_INT quantity_columns;
  MKL_INT quantity_nnz;
  sparse_matrix_t  quantity_matrix;

  /** ---------------------------------------------------------------------------
   ** Shipdate Matrix
   ** -------------------------------------------------------------------------*/
  //CSR
  __declspec(align(MEM_LINE_SIZE))  float* shipdate_csr_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* shipdate_JA;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* shipdate_IA;
  //CSR
  __declspec(align(MEM_LINE_SIZE))  float* shipdate_csc_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* shipdate_JA_csc;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* shipdate_IA_csc;
  //COMMON
  MKL_INT shipdate_rows;
  MKL_INT shipdate_columns;
  MKL_INT shipdate_nnz;
  sparse_matrix_t  shipdate_matrix;

  /* ---------------------------------------------------------------------------
   ** Projection Matrix
   ** -------------------------------------------------------------------------*/
  //CSR
  __declspec(align(MEM_LINE_SIZE))  float* projection_csr_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* projection_JA;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* projection_IA;
  //COMMON
  MKL_INT projection_rows;
  MKL_INT projection_columns;
  MKL_INT projection_nnz;
  sparse_matrix_t  projection_matrix;

  /* ---------------------------------------------------------------------------
   ** Aggregation Matrix
   ** -------------------------------------------------------------------------*/
  //CSR
  __declspec(align(MEM_LINE_SIZE)) float* aggregation_csr_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* aggregation_JA;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* aggregation_IA;
  //COMMON
  MKL_INT aggregation_rows;
  MKL_INT aggregation_columns;
  MKL_INT aggregation_nnz;

  /* ---------------------------------------------------------------------------
   ** Intermediate Matrix
   ** -------------------------------------------------------------------------*/
  //CSR
  __declspec(align(MEM_LINE_SIZE)) float* intermediate_csr_values = NULL;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* intermediate_JA;
  __declspec(align(MEM_LINE_SIZE))  MKL_INT* intermediate_IA;
  //COMMON
  MKL_INT intermediate_rows;
  MKL_INT intermediate_columns;
  MKL_INT intermediate_nnz;
  sparse_matrix_t  intermediate_matrix;

  /* ---------------------------------------------------------------------------
   ** Vectors
   ** -------------------------------------------------------------------------*/
  __declspec(align(MEM_LINE_SIZE))  float* bang_vector;
  __declspec(align(MEM_LINE_SIZE))  float* aggregation_vector;
  __declspec(align(MEM_LINE_SIZE))  float* final_vector;


  //conversion status from csr arrays into mkl sparse_matrix_t 
  sparse_status_t status_to_csr;

  /** ---------------------------------------------------------------------------
   ** Populate Return Flag Matrix
   ** -------------------------------------------------------------------------*/
  //read return flag
  tbl_read( table , 9, &returnFlag_nnz, &returnFlag_rows, &returnFlag_columns, &returnFlag_csr_values, &returnFlag_JA, &returnFlag_IA);

  //read_from_mx(return_flag, &return_flag_csr_values, &return_flag_JA, &return_flag_IA, &return_flag_nnz, &return_flag_rows, &return_flag_columns);

  // Memory Allocation
  return_flag_csc_values = (float*) mkl_malloc (( return_flag_nnz * sizeof(float) ), MEM_LINE_SIZE );
  return_flag_JA_csc = (MKL_INT*) mkl_malloc (( return_flag_nnz * sizeof(MKL_INT) ), MEM_LINE_SIZE );
  return_flag_IA_csc = (MKL_INT*) mkl_malloc ((( return_flag_nnz+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  // Convert from CSR to CSC
  mkl_scsrcsc(job_csr_csc, &return_flag_nnz, return_flag_csr_values, return_flag_JA, return_flag_IA, return_flag_csc_values, return_flag_JA_csc, return_flag_IA_csc, &status_convert_to_csc);

  /** ---------------------------------------------------------------------------
   ** Populate Line Status Matrix
   ** -------------------------------------------------------------------------*/
  //read line status
  tbl_read( table , 10, &lineStatus_nnz, &lineStatus_rows, &lineStatus_columns , &lineStatus_csr_values, &lineStatus_JA, &lineStatus_IA);

  // read_from_mx(line_status, &line_status_csr_values, &line_status_JA, &line_status_IA, &line_status_nnz, &line_status_rows, &line_status_columns);

  // Memory Allocation
  line_status_csc_values = (float*) mkl_malloc (( line_status_nnz * sizeof(float) ), MEM_LINE_SIZE );
  line_status_JA_csc = (MKL_INT*) mkl_malloc (( line_status_nnz * sizeof(MKL_INT) ), MEM_LINE_SIZE );
  line_status_IA_csc = (MKL_INT*) mkl_malloc ((( line_status_nnz+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  // Convert from CSR to CSC
  mkl_scsrcsc(job_csr_csc, &line_status_nnz, line_status_csr_values, line_status_JA, line_status_IA, line_status_csc_values, line_status_JA_csc, line_status_IA_csc, &status_convert_to_csc);

  /** ---------------------------------------------------------------------------
   ** Populate Quantity Matrix
   ** -------------------------------------------------------------------------*/
  //read quantity
  tbl_read_measure( table , 5, &quantity_nnz, &quantity_rows, &quantity_columns , &quantity_csr_values, &quantity_JA, &quantity_IA);
  // read_from_mx(quantity, &quantity_csr_values, &quantity_JA, &quantity_IA, &quantity_nnz, &quantity_rows, &quantity_columns);

  // Memory Allocation
  quantity_csc_values = (float*) mkl_malloc (( quantity_nnz * sizeof(float) ), MEM_LINE_SIZE );
  quantity_JA_csc = (MKL_INT*) mkl_malloc (( quantity_nnz * sizeof(MKL_INT) ), MEM_LINE_SIZE );
  quantity_IA_csc = (MKL_INT*) mkl_malloc ((( quantity_nnz+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  // Convert from CSR to CSC
  mkl_scsrcsc(job_csr_csc, &quantity_nnz, quantity_csr_values, quantity_JA, quantity_IA, quantity_csc_values, quantity_JA_csc, quantity_IA_csc, &status_convert_to_csc);

  //        convert via sparseBLAS API to Handle containing internal data for 
  //        subsequent Inspector-executor Sparse BLAS operations.
  status_to_csr = mkl_sparse_s_create_csr ( &quantity_matrix , SPARSE_INDEX_BASE_ZERO, 
      quantity_rows, quantity_columns, quantity_IA, quantity_IA+1, quantity_JA, quantity_csr_values );

  /** ---------------------------------------------------------------------------
   ** Populate Shipdate Matrix
   ** -------------------------------------------------------------------------*/
  //read shipdate
  tbl_read( table , 11, &shipdate_nnz, &shipdate_rows, &shipdate_columns , &shipdate_csr_values, &shipdate_JA, &shipdate_IA);

  // read_from_mx(shipdate_gt, &shipdate_gt_csr_values, &shipdate_gt_JA, &shipdate_gt_IA, &shipdate_gt_nnz, &shipdate_gt_rows, &shipdate_gt_columns);

  // Memory Allocation
  shipdate_csc_values = (float*) mkl_malloc (( shipdate_gt_nnz * sizeof(float) ), MEM_LINE_SIZE );
  shipdate_JA_csc = (MKL_INT*) mkl_malloc (( shipdate_gt_nnz * sizeof(MKL_INT) ), MEM_LINE_SIZE );
  shipdate_IA_csc = (MKL_INT*) mkl_malloc (((shipdate_gt_nnz+1) * sizeof(MKL_INT)), MEM_LINE_SIZE );

  // Convert from CSR to CSC
  mkl_scsrcsc(job_csr_csc, &shipdate_nnz, shipdate_csr_values, shipdate_JA, shipdate_IA, shipdate_csc_values, shipdate_JA_csc, shipdate_IA_csc, &status_convert_to_csc);

  //        convert via sparseBLAS API to Handle containing internal data for
  //        subsequent Inspector-executor Sparse BLAS operations.
  status_to_csr = mkl_sparse_s_create_csr ( &shipdate_matrix , SPARSE_INDEX_BASE_ZERO,
      shipdate_rows, shipdate_columns, shipdate_IA, shipdate_IA+1, shipdate_JA, shipdate_csr_values );

  /** ---------------------------------------------------------------------------
   ** Auxiliar Vars
   ** -------------------------------------------------------------------------*/
  sparse_status_t selection_result;
  sparse_status_t aggregation_result;
  sparse_status_t intermediate_result;
  sparse_status_t final_result;

  struct matrix_descr descrA;
  descrA.type = SPARSE_MATRIX_TYPE_GENERAL;

  /** ---------------------------------------------------------------------------
   ** Populate Vectors
   ** -------------------------------------------------------------------------*/
  bang_vector = (float*) mkl_malloc ( (quantity_columns * sizeof(float)), MEM_LINE_SIZE );
  aggregation_vector = (float*) mkl_malloc ( (quantity_columns * sizeof(float)), MEM_LINE_SIZE );

  /** ---------------------------------------------------------------------------
   ** ---------------------------------------------------------------------------
   ** ---------------------------------------------------------------------------
   ** ---------------------------------------------------------------------------
   ** ---------------------------------------------------------------------------
   ** START TIME MEASUREMENT
   ** ---------------------------------------------------------------------------
   ** ---------------------------------------------------------------------------
   ** ---------------------------------------------------------------------------
   ** ---------------------------------------------------------------------------
   ** ---------------------------------------------------------------------------
   ** -------------------------------------------------------------------------*/
  GET_TIME(global_time_start);

  csr_mx_selection_and(
      shipdate_csr_values, shipdate_JA, shipdate_IA,
      shipdate_nnz, shipdate_rows, shipdate_columns,
      GREATER_EQ , "1998-08-28", LESS_EQ , "1998-12-01",
      &selection_csr_values, &selection_JA, &selection_IA
      &selection_nnz, &selection_rows, &selection_columns
      );

  status_to_csr = mkl_sparse_s_create_csr ( &projection_matrix , SPARSE_INDEX_BASE_ZERO, projection_rows, projection_columns, projection_IA, projection_IA+1, projection_JA, projection_csr_values );

  // compute projection = return_flag krao line_status
  csc_csr_krao(
      return_flag_csc_values, return_flag_JA_csc, return_flag_IA_csc,
      return_flag_nnz, return_flag_rows, return_flag_columns,
      line_status_csc_values, line_status_JA_csc, line_status_IA_csc ,
      line_status_nnz, line_status_rows, line_status_columns,
      &projection_csr_values, &projection_JA, &projection_IA,
      &projection_nnz, &projection_rows, &projection_columns
      );

  // compute aggregation = quantity * bang
  aggregation_result = mkl_sparse_s_mv ( SPARSE_OPERATION_NON_TRANSPOSE, 1.0, quantity_matrix , descrA, bang_vector, 1.0,  aggregation_vector);

  // compute intermediate_result = projection * selection
  intermediate_result = mkl_sparse_spmm ( SPARSE_OPERATION_NON_TRANSPOSE, 
      projection_matrix,
      selection_matrix, 
      &intermediate_matrix);

  // compute final_result = intermediate_result * aggregation
  final_result = mkl_sparse_s_mv ( SPARSE_OPERATION_NON_TRANSPOSE, 1.0, intermediate_matrix , descrA, aggregation_vector, 1.0,  final_vector);

  ////////////////////////
  // STOP TIME MEASUREMENT
  ////////////////////////
  GET_TIME(global_time_stop);
  writeResults( argv[1] );

  return 0;

}

