
#!/bin/bash
# qsub -q short_cpuQ -l select=2:ncpus=2 -o parallel_implementation/output/out -e parallel_implementation/output/err ./parallel_implementation/hc_batch_qsub.sh

repeat_executions=1

start_num_of_processes=2
end_num_of_processes=4
step=1

echo -e "Running ${repeat_executions} executions with processes from ${start_num_of_processes} to ${end_num_of_processes} skipping ${step} each step...\n"

for ((j=1;j<=repeat_executions;j++))
do 
    echo -e "--- Execution ${j} of ${repeat_executions}...\n"
    for ((i=start_num_of_processes;i<=end_num_of_processes;i+=step)) 
    do
        echo -e "Running with $i nodes..."
        touch parallel_implementation/output/reference.txt

        if [ -z parallel_implementation/output/out ]; then 
            rm parallel_implementation/output/out
        fi

        qsub -q short_cpuQ -l select=$i:ncpus=$i -o parallel_implementation/output/out -e parallel_implementation/output/err ./parallel_implementation/hc_batch_qsub.sh

        while ! test parallel_implementation/output/out -nt parallel_implementation/output/reference.txt; do
            sleep 3; 
        done

        rm parallel_implementation/output/reference.txt

        echo -e "Output of execution:"
        cat parallel_implementation/output/out
        
        echo -e "Done with $i nodes\n"
    done
done