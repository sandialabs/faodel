FADO: Faodel Container for Apache Arrow Data
============================================

Apache Arrow is a C++ library that makes it easy for different applications
to process tabular data. Arrow is popular in the data science community
because it provides a robust data model for representing structured data,
and compute primitives that are map to parallel hardware.


The intent of the faodel-arrow portion of this repo is to make it easier
for users to serialize one or more Apache Arrow tables into a Lunasa
Data Object (LDO) that can easily be moved between nodes in a Kelpie pool. The
`ArrowDataObject` (FADO) provides a convenient wrapper to ease
the transition between in-memory Arrow tables and a contiguous LDO.