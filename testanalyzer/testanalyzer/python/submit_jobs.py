import os
import subprocess
import textwrap
import time
import shutil
import getpass
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

# --- CONFIGURATION ---
NUM_CPUS = 10                      # Lowered to 4 to reduce AFS pressure
FILES_PER_JOB = 2
CONFIG_FILE = "test.py"
FINAL_OUTPUT_NAME = "merged_v4.root"
OUTPUT_DIR = str(Path.cwd() / "job_outputs")
# Use /tmp for live logging to avoid AFS I/O errors
LOCAL_TEMP_LOG_DIR = f"/afs/cern.ch/user/v/vle/work/CMSSW_15_0_19/src/testanalyzer/testanalyzer/python/job_logs_tmp/{getpass.getuser()}_job_logs_{int(time.time())}"
MASTER_LOG_NAME = f"master_log_{time.strftime('%Y%m%d_%H%M%S')}.log"
# ---------------------

def run_cmsrun_job(job_id, files):
    output_root = f"{OUTPUT_DIR}/output_{job_id}.root"
    log_path = f"{LOCAL_TEMP_LOG_DIR}/job_{job_id}.log"
    input_files_str = ",".join(files)
    
    run_command = f"cmsRun {CONFIG_FILE} inputFiles={input_files_str} outputFile={output_root}"

    # Get proxy path dynamically
    uid = os.getuid()
    proxy_path = f"/afs/cern.ch/user/v/vle/x509up_u{uid}"

    cmd = textwrap.dedent(f"""\
        cmssw-el9 << EOF_SINGULARITY
        cd {Path.cwd()}
        source /cvmfs/cms.cern.ch/cmsset_default.sh
        export X509_USER_PROXY={proxy_path}
        cmsenv
        {run_command}
        EOF_SINGULARITY
    """)
    
    print(f" >> Starting Job {job_id}...", flush=True) 
    
    # Ensure log directory exists on local disk
    os.makedirs(LOCAL_TEMP_LOG_DIR, exist_ok=True)

    with open(log_path, "w") as f:
        f.write(f"JOB ID: {job_id}\nFILES: {input_files_str}\nPROXY: {proxy_path}\n{'='*80}\n")
        f.flush()
        result = subprocess.run(cmd, shell=True, stdout=f, stderr=subprocess.STDOUT, executable="/bin/bash")
    
    if result.returncode == 0:
        return output_root
    else:
        print(f" !! Job {job_id} FAILED. Check: {log_path}")
        return None

def main():
    if not os.path.exists(OUTPUT_DIR): os.makedirs(OUTPUT_DIR)
    if not os.path.exists(LOCAL_TEMP_LOG_DIR): os.makedirs(LOCAL_TEMP_LOG_DIR)

    # 1. Get file list
    all_files = []
    file_txts = [
        '/afs/cern.ch/user/v/vle/work/CMSSW_15_0_19/src/testanalyzer/testanalyzer/file_list_0p2to10.txt',
        '/afs/cern.ch/user/v/vle/work/CMSSW_15_0_19/src/testanalyzer/testanalyzer/file_list_0p2to200.txt',
        '/afs/cern.ch/user/v/vle/work/CMSSW_15_0_19/src/testanalyzer/testanalyzer/file_list_200to500.txt',
        '/afs/cern.ch/user/v/vle/work/CMSSW_15_0_19/src/testanalyzer/testanalyzer/file_list_500to5000.txt'
    ]
    for txt in file_txts:
        if os.path.exists(txt):
            with open(txt) as f:
                all_files += f.read().splitlines()[:5]

    chunks = [all_files[i:i + FILES_PER_JOB] for i in range(0, len(all_files), FILES_PER_JOB)]
    print(f"Submitting {len(chunks)} jobs. Live logs: {LOCAL_TEMP_LOG_DIR}")

    # 2. Parallel execution
    results = []
    try:
        with ThreadPoolExecutor(max_workers=NUM_CPUS) as executor:
            futures = [executor.submit(run_cmsrun_job, i, chunk) for i, chunk in enumerate(chunks)]
            for future in futures:
                res = future.result()
                if res: results.append(res)
    except KeyboardInterrupt:
        print("\n !! Cancelled.")

    # 3. Merging Logs (Moving from /tmp to AFS)
    print(f"Combining logs into {MASTER_LOG_NAME}...")
    try:
        with open(MASTER_LOG_NAME, 'w') as master:
            for i in range(len(chunks)):
                log_part = f"{LOCAL_TEMP_LOG_DIR}/job_{i}.log"
                if os.path.exists(log_part):
                    with open(log_part, 'r') as f:
                        master.write(f.read())
                        master.write("\n\n")
        # Clean up local logs
        shutil.rmtree(LOCAL_TEMP_LOG_DIR)
    except Exception as e:
        print(f"Error finalizing logs: {e}")

    # 4. Hadd
    if results:
        print(f"Merging {len(results)} files...")
        hadd_cmd = f"cmssw-el9 << EOF_SINGULARITY\ncd {Path.cwd()}\ncmsenv\nhadd -f {FINAL_OUTPUT_NAME} {' '.join(results)}\nEOF_SINGULARITY"
        subprocess.run(hadd_cmd, shell=True, executable="/bin/bash")

if __name__ == "__main__":
    main()