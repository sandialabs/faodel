############# Function to generate XDR source and header Files ###############

function (nntiProcessXDR inpath outpath)

   GET_FILENAME_COMPONENT(file ${inpath} NAME_WE)

   add_custom_command(
     OUTPUT ${outpath}/${file}.c
     COMMAND rpcgen -Cc ${inpath}
        | sed -e "\"s#include.*${file}.*#include <nnti/${file}.h>#\""
                > ${outpath}/${file}.c
     DEPENDS ${inpath} ${outpath}/${file}.h)


   add_custom_command(
     OUTPUT ${outpath}/${file}.h
     COMMAND rpcgen -Ch ${inpath}
        | sed -e "\"s#rpc/rpc.h#${file}.h#\""
        | sed -e "\"s#include <${file}.h>#include <nnti/${file}.h>#\""
        | perl -pe \"BEGIN{undef $$/\;} s/\(enum\\s\\w+\\s\\{\\n\(\\s*.*?,\\n\)*?\\s*.*?\),\(\\n\\s*\\}\;\)/\\1\\3/smg\"
        > ${outpath}/${file}.h
     DEPENDS ${inpath})

   # Need target to force construction of nssi_types_xdr.{c,h} and nnti_xdr.{c,h}
   add_custom_target(generate-${file} DEPENDS ${outpath}/${file}.c ${outpath}/${file}.h)

   set_source_files_properties(
      ${outpath}/${file}.h
      PROPERTIES GENERATED TRUE
   )
   set_source_files_properties(
      ${outpath}/${file}.c
      PROPERTIES GENERATED TRUE
   )
endfunction (nntiProcessXDR)
