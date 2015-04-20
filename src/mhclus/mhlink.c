/**
 * @file mhlink.c
 * @author Gibran Fuentes Pineda <gibranfp@turing.iimas.unam.mx>
 * @date 2013
 *
 * @section GPL
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @brief Functions for performing single-link clustering based on Min-Hashing
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>
#include "types.h"
#include "mhlink.h"

/**
 * @brief Converts clusters (lists of ids) to lists of items.
 *
 * @param listdb Database of lists
 * @param cluster Clusters given as lists of list ids
 * @param model Converted clusters (lists of items)
 */
ListDB mhlink_make_model(ListDB *listdb, ListDB *clusters)
{
     ListDB models;
     models.size = clusters->size;
     models.dim = listdb->dim;
     models.lists = (List *) calloc(models.size, sizeof(List));

     uint i, j;
     for (i = 0; i < clusters->size; i++){
	  for (j = 0; j < clusters->lists[i].size; j++)
	       list_append(&models.lists[i], &listdb->lists[clusters->lists[i].data[j].item]);
	  list_sort_by_item(&models.lists[i]);
	  list_unique(&models.lists[i]);
	  list_sort_by_frequency_back(&models.lists[i]);
     }

     return models;
}

/**
 * @brief Checks a hash bucket for similar lists to be merged in a
 *        cluster.
 *
 * @param listdb Database of lists
 * @param clusters Generated clusters
 * @param index Index of the list in the hash table
 * @param Listid ID of the list to be added
 * @param hash_table Hash table
 * @param checked Keeps track of the already checked lists
 * @param clus_table Keeps track of the cluster to which each list is
 *                   assigned
 * @param ovr_th Threshold to merge clusters
 */
void mhlink_add_neighbors(ListDB *listdb, ListDB *clusters, uint listid, List *items, uint *checked, 
			  uint *clus_table, double (*sim)(List *, List *), double thres)
{
     uint i;
     for (i = 0; i < items->size; i++) {	       
	  if (items->data[i].item != listid && sim(&listdb->lists[listid], &listdb->lists[items->data[i].item]) > thres){
	       if (checked[items->data[i].item] == 0){ // list hasn't been checked

		    // Add item to cluster
		    Item new_item = {items->data[i].item, 1};
		    list_push(&clusters->lists[clus_table[listid]], new_item);

		    // mark list as checked
		    checked[items->data[i].item] = 1;

		    // assigning current id to new item
		    clus_table[items->data[i].item] = clus_table[listid];
	       } else if (clus_table[items->data[i].item] != clus_table[listid]) {
		    // get min and max between cluster ids
		    uint max_cluster_id = max(clus_table[items->data[i].item], clus_table[listid]);
		    uint min_cluster_id = min(clus_table[items->data[i].item], clus_table[listid]);	

		    // Merge clusters
		    list_append(&clusters->lists[min_cluster_id], &clusters->lists[max_cluster_id]);

		    uint j;
		    for (j = 0; j < clusters->lists[max_cluster_id].size; j++)
			 // reassigning ids to cluster with largest id
			 clus_table[clusters->lists[max_cluster_id].data[j].item] = min_cluster_id;
		    
		    // Destroy cluster with largest id
		    list_destroy(&clusters->lists[max_cluster_id]);
	       }
	  }
     }
}

/**
 * @brief Single-link clustering based on Min-Hashing.
 *
 * @param listdb Database of lists to be hashed
 * @param table_size Number of buckets in the hash table
 * @param tuple_size Number of MinHash values per tuple
 * @param sim Similarity function for adding list to a cluster
 * @param thres Threshold for adding list to a cluster
 */
ListDB mhlink_cluster(ListDB *listdb, uint table_size, uint number_of_tuples,
		      uint tuple_size, double (*sim)(List *, List *), double thres)
{
     uint i, j;
     uint *checked = (uint *) calloc(listdb->size, sizeof(uint));
     uint *clus_table = (uint *) malloc(listdb->size * sizeof(uint));
     uint *indices = (uint *) malloc(listdb->size * sizeof(uint));
     HashTable hash_table = mh_create(table_size, tuple_size, listdb->dim);
     ListDB clusters;  
     listdb_init(&clusters);

     for (i = 0; i < number_of_tuples; i++){// computes each hash table
	  printf("Table %u/%u: %u random permutations for %u lists\r",
	  	 i + 1, number_of_tuples, tuple_size, listdb->size);
	  
	  // stores lists in the hash table
	  mh_generate_permutations(listdb->dim, tuple_size, hash_table.permutations);
	  mh_store_listdb(listdb, &hash_table, indices);

	  // sorts used items in ascending order
	  list_sort_by_item(&hash_table.used_buckets);
	  
	  for (j = 0; j < listdb->size; j++){
	       if (checked[j] == 0){// list hasn't been checked
		    // a new cluster is formed
		    List new_cluster;
		    list_init(&new_cluster);
			  
		    Item new_item = {j, 1};
		    list_push(&new_cluster, new_item);
			  
		    clus_table[j] = clusters.size;
		    listdb_push(&clusters, &new_cluster);

		    checked[j] = 1;
	       }

	       // assign items in the same bucket to the same cluster
	       mhlink_add_neighbors(listdb, &clusters, j, &hash_table.buckets[indices[j]].items, 
				    checked, clus_table, sim, thres);

	       // Freeing up bucket
	       list_destroy(&hash_table.buckets[indices[j]].items);
	  }
	  
	  // cleaning list of used buckets
	  list_destroy(&hash_table.used_buckets);
     }

     printf("\n");

     free(indices);
     free(checked);
     free(clus_table);

     return clusters;
}
