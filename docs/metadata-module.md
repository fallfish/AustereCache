## Metadata Module

### MetadataModule

Functions:

**dedup()**

1. Checks the LBA index
2. Checks the CA index
3. if CA index OK -> Verify metadata by the `MetaVerification` class. It reads the metadata from the SSD to check whether the metadata (LBA and CA) matches the Chunk metadata.
4. Finally it checks whether the write is:
   + a duplicated write (Same Addr, same content)
   + duplicated content (Diff Addr, same content)
   + Not duplicated (Diff content)

**lookup()**

1. Checks the LBA index
2. If LBA index OK -> Checks the CA index
3. If CA index OK -> Verify metadata
4. (?)



### LBAIndex: lba_hash -> ca_hashs

Members:

`n_bits_per_key`:  Length of signature



Functions:

**lookup(lba_hash, &ca_hash)**

 `lba_hash` is the hash value of the LBA; `ca_hash` is the hash value of the fingerprint. 

This function uses some most significant bits of `lba_hash` to find the bucket **LBABucket**, and then find the rest least significant bits in the corresponding bucket as the *key*. Also, it will update `ca_hash` (as the *value*) by the `lookup()` function of **LBABucket** if the CA is found.

**promote(lba_sig)**

It assumes that `lba_sig` exists in the bucket. "Promote" means calling the `_cache_policy` to improve the priority of the accessed value. (e.g. LRU)

**update(lba_hash, ca_hash)**

This function selects the bucket by taking the `bucket_id` from MSBs of `lba_hash`. Then updates the mapping `lba_hash -> ca_hash` to the bucket. 

It may cause index eviction (by **hash collision**), when the `lba_sig` is correct but `ca_hash` is wrong.



### CAIndex: ca_hash -> ssd_location

**lookup()**, **promote()**, **update()**

Almost the same as those functions in **LBAIndex**.