
LINK_LIBRARIES(
    persist-ipfs
    persist
    atomspace
)

# The seven unit tests, ported over from the
# atomspace/persist/sql/multi-driver unit tests.
ADD_CXXTEST(BasicSaveUTest)
ADD_CXXTEST(ValueSaveUTest)
ADD_CXXTEST(PersistUTest)
ADD_CXXTEST(FetchUTest)
TARGET_LINK_LIBRARIES(FetchUTest execution)
ADD_CXXTEST(DeleteUTest)
ADD_CXXTEST(MultiPersistUTest)
ADD_CXXTEST(MultiUserUTest)
