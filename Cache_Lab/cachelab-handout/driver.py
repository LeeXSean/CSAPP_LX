#!/usr/bin/python3
#
# driver.py - Python 3 compatible version
#
import subprocess
import re
import os
import sys
import optparse

def computeMissScore(miss, lower, upper, full_score):
    if miss <= lower:
        return full_score
    if miss >= upper: 
        return 0

    score = (miss - lower) * 1.0 
    score_range = (upper - lower) * 1.0
    return round((1 - score / score_range) * full_score, 1)

def main():
    maxscore = {}
    maxscore['csim'] = 27
    maxscore['transc'] = 1
    maxscore['trans32'] = 8
    maxscore['trans64'] = 8
    maxscore['trans61'] = 10

    p = optparse.OptionParser()
    p.add_option("-A", action="store_true", dest="autograde", 
                 help="emit autoresult string for Autolab")
    opts, args = p.parse_args()
    autograde = opts.autograde

    # Part A: Cache Simulator
    print("Part A: Testing cache simulator")
    print("Running ./test-csim")
    p = subprocess.Popen("./test-csim", 
                         shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout_data, _ = p.communicate()
    stdout_data = stdout_data.decode('utf-8') # Python 3 需要解码字节流

    resultsim = []
    lines = stdout_data.split('\n')
    for line in lines:
        if "TEST_CSIM_RESULTS" in line:
            resultsim = re.findall(r'(\d+)', line)
        else:
            print(line)

    # Part B: Transpose function
    print("Part B: Testing transpose function")
    
    # 32x32
    print("Running ./test-trans -M 32 -N 32")
    p = subprocess.Popen("./test-trans -M 32 -N 32 | grep TEST_TRANS_RESULTS", 
                         shell=True, stdout=subprocess.PIPE)
    out, _ = p.communicate()
    result32 = re.findall(r'(\d+)', out.decode('utf-8'))
    
    # 64x64
    print("Running ./test-trans -M 64 -N 64")
    p = subprocess.Popen("./test-trans -M 64 -N 64 | grep TEST_TRANS_RESULTS", 
                         shell=True, stdout=subprocess.PIPE)
    out, _ = p.communicate()
    result64 = re.findall(r'(\d+)', out.decode('utf-8'))
    
    # 61x67
    print("Running ./test-trans -M 61 -N 67")
    p = subprocess.Popen("./test-trans -M 61 -N 67 | grep TEST_TRANS_RESULTS", 
                         shell=True, stdout=subprocess.PIPE)
    out, _ = p.communicate()
    result61 = re.findall(r'(\d+)', out.decode('utf-8'))
    
    # 计算分数
    try:
        csim_cscore = int(resultsim[0]) if resultsim else 0
        miss32 = int(result32[1]) if len(result32) > 1 else 2**31-1
        miss64 = int(result64[1]) if len(result64) > 1 else 2**31-1
        miss61 = int(result61[1]) if len(result61) > 1 else 2**31-1
        
        res32_ok = int(result32[0]) if result32 else 0
        res64_ok = int(result64[0]) if result64 else 0
        res61_ok = int(result61[0]) if result61 else 0

        trans32_score = computeMissScore(miss32, 300, 600, maxscore['trans32']) * res32_ok
        trans64_score = computeMissScore(miss64, 1300, 2000, maxscore['trans64']) * res64_ok
        trans61_score = computeMissScore(miss61, 2000, 3000, maxscore['trans61']) * res61_ok
        total_score = csim_cscore + trans32_score + trans64_score + trans61_score
    except Exception as e:
        print(f"Error parsing results: {e}")
        total_score = 0

    # 打印总结
    print("\nCache Lab summary:")
    print("%-22s%8s%10s%12s" % ("", "Points", "Max pts", "Misses"))
    print("%-22s%8.1f%10d" % ("Csim correctness", csim_cscore, maxscore['csim']))

    for label, score, max_p, m in [("Trans perf 32x32", trans32_score, maxscore['trans32'], miss32),
                                   ("Trans perf 64x64", trans64_score, maxscore['trans64'], miss64),
                                   ("Trans perf 61x67", trans61_score, maxscore['trans61'], miss61)]:
        m_str = str(m) if m != 2**31-1 else "invalid"
        print("%-22s%8.1f%10d%12s" % (label, score, max_p, m_str))

    print("%22s%8.1f%10d" % ("Total points", total_score, 61))

if __name__ == "__main__":
    main()
