;
; OpenCog SQL Persistance module
;

(define-module (opencog persist-ipfs))


(use-modules (opencog))
(use-modules (opencog ipfs-config))
(load-extension
	(string-append opencog-ext-path-persist-ipfs "libpersist-ipfs")
	"opencog_persist_ipfs_init")

(export ipfs-clear-stats ipfs-close ipfs-load ipfs-open
	ipfs-store ipfs-stats
	ipfs-atom-cid ipfs-fetch-atom
	ipfs-atomspace-cid ipns-atomspace-cid)

(set-procedure-property! ipfs-clear-stats 'documentation
"
 ipfs-clear-stats - reset the performance statistics counters.
    This will zero out the various counters used to track the
    performance of the SQL backend.  Statistics will continue to
    be accumulated.
")

(set-procedure-property! ipfs-close 'documentation
"
 ipfs-close - close the currently open SQL backend.
    Close open connections to the currently-open backend, afterflushing
    any pending writes in the write queues. After the close, atoms can
    no longer be stored to or fetched from the database.
")

(set-procedure-property! ipfs-load 'documentation
"
 ipfs-load - load all atoms in the database.
    This will cause ALL of the atoms in the open database to be loaded
    into the atomspace. This can be a very time-consuming operation.
    In normal operation, it is rarely necessary to load all atoms;
    atoms can always be fetched and stored one at a time, on demand.
")

(set-procedure-property! ipfs-open 'documentation
"
 ipfs-open URL - Open a connection to an IPFS server.

  The URL must be one of these formats:
     ipfs:///KEY-NAME
     ipfs://HOSTNAME/KEY-NAME
     ipfs://HOSTNAME:PORT/KEY-NAME

  If no hostname is specified, its assumed to be 'localhost'. If no port
  is specified, its assumed to be 5001.

  Examples of use with valid URL's:
     (ipfs-open \"ipfs:///atomspace-test\")
     (ipfs-open \"ipfs://localhost/atomspace-test\")
     (ipfs-open \"ipfs://localhost:5001/atomspace-test\")
")

(set-procedure-property! ipfs-store 'documentation
"
 ipfs-store - Store all atoms in the atomspace to the database.
    This will dump the ENTIRE contents of the atomspace to the databse.
    Depending on the size of the database, this can potentially take a
    lot of time.  During normal operation, a bulk-save is rarely
    required, as individual atoms can always be stored, one at a time.
")

(set-procedure-property! ipfs-stats 'documentation
"
 ipfs-stats - report performance statistics.
    This will cause some database performance statistics to be printed
    to the stdout of the server. These statistics can be quite arcane
    and are useful primarily to the developers of the database backend.
")

(set-procedure-property! ipfs-atom-cid 'documentation
"
 ipfs-atom-cid ATOM - Return the string CID of the IPFS entry of ATOM.

     For example:
         `(ipfs-atom-cid (Concept \"example concept\"))`
     should always return
        \"/ipfs/QmTBUxX48jRZPwAU3dEgPQm4bShxW2ED3gXTHM78gvqugB\"
     This can be used to examine that Atom and to explore it
     using an IPFS explorer.

     See also `ipfs-fetch-atom` and `ipfs-atomspace-cid`.
")

(set-procedure-property! ipfs-fetch-atom 'documentation
"
 ipfs-fetch-atom CID - Return the Atom associated to the CID.

     For example:
        `(ipfs-fetch-atom \"QmTBUxX48jRZPwAU3dEgPQm4bShxW2ED3gXTHM78gvqugB\")`
     should always return
        `(Concept \"example concept\")`

     See also `ipfs-atom-cid` for the inverse operation.
")

(set-procedure-property! ipfs-load-atomspace 'documentation
"
 ipfs-load-atomspace CID - Load all Atoms from the CID into the AtomSpace.

   For example:
      `(ipfs-load-atomspace \"QmT9tZttJ4gVZQwVFHWTmJYqYGAAiKEcvW9k98T5syYeYU\")`
   should load the atomspace from `basic.scm` example.

   See also `ipfs-fetch-atom` for loading individual atoms.
")

(set-procedure-property! ipfs-atomspace-cid 'documentation
"
 ipfs-atomspace-cid - Return the string CID of the IPFS entry of the
     current AtomSpace.  An example of a returned value is
        \"/ipfs/QmT9tZttJ4gVZQwVFHWTmJYqYGAAiKEcvW9k98T5syYeYU\"
     This can be used to access the AtomSpace and to explore it
     using an IPFS explorer. One public explorer is
        `https://explore.ipld.io/#/explore/`

     See also `ipns-atomspace-cid`.
")

(set-procedure-property! ipns-atomspace-cid 'documentation
"
 ipns-atomspace-cid - Return the string CID of the IPNS entry of the
     current AtomSpace.  An example of a returned value is
        \"/ipns/QmVkzxhCMDYisZ2QEMA5WYTjrZEPVbETZg5rehsijUxVHx\"
     This can be used to access the AtomSpace and to explore it
     using an IPFS explorer.  For example, the actual current
     atomspace can be found by issuing the bash shell command
        `ipfs name resolve /ipns/Qm...VHx`
     The result of this resolution should be equal to what the
     scheme command `(ipfs-atomspace-cid)` is returning.
")
