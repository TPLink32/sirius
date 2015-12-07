mpicc -o shm_mm_test shm_mm_test.c -I.. -L.. -lshm_transport
mpicc -o shm_queue_test shm_queue_test.c -I.. -L.. -lshm_transport
mpicc -o meta_region_test meta_region_test.c -I.. -L.. -lshm_transport -I../../adios.staging/ -I../../adios.staging/src -L ../../adios.staging/src -ladios
mpicc -o data_q_test_w data_q_test_w.c -I.. -L.. -lshm_transport -I../../adios.staging/ -I../../adios.staging/src -L ../../adios.staging/src -ladios
mpicc -o data_q_test_r data_q_test_r.c -I.. -L.. -lshm_transport -I../../adios.staging/ -I../../adios.staging/src -L ../../adios.staging/src -ladios
mpicc -o shm_reader_test shm_reader_test.c -I.. -L.. -lshm_transport -I../../adios.staging/ -I../../adios.staging/src -L ../../adios.staging/src -ladios
mpicc -g -o simple_reader simple_reader.c -I.. -L.. -lshm_transport -I../../adios.staging/ -I../../adios.staging/src -L ../../adios.staging/src -ladios
mpicc -g -o gts_reader gts_reader.c -I.. -L.. -lshm_transport -I../../adios.staging/ -I../../adios.staging/src -L ../../adios.staging/src -ladios

cp shm_mm_test /tmp/work/zf2/test
cp shm_queue_test /tmp/work/zf2/test
cp meta_region_test /tmp/work/zf2/test
cp data_q_test_w /tmp/work/zf2/test 
cp data_q_test_r /tmp/work/zf2/test 
cp shm_reader_test /tmp/work/zf2/test 
cp simple_reader /tmp/work/zf2/test 
cp gts_reader /tmp/work/zf2/test/gts 
