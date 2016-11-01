import sys
from random import random, randint

def dateval_to_str(value):
    year = value // 12
    month = (value - (year*12)) + 1
    result = "01/%d/%d" % (month, year)
    return result

def generate(output_name, src_count=5, req_count=5, bpl_count=2, duration=24, max_tenor=100000):
    max_tenor = min(duration, max_tenor)
    start_date = 2015 * 12
    end_date = start_date + duration
    amount_step_size = 50
    max_amount = 5000
    min_amount = 500
    amount_steps = (max_amount - min_amount) // amount_step_size

    sources = []
    requirements = []
    balancepools = []
    src_filename = "data/%s_sources.csv" % output_name
    req_filename = "data/%s_requirements.csv" % output_name
    bpl_filename = "data/%s_balancepools.csv" % output_name
    src_file = open(src_filename, "w")
    req_file = open(req_filename, "w")
    bpl_file = open(bpl_filename, "w")
    src_file.write("Id,Segment,Start Date,Tenor,Amount,Source Type,Source Type Category,Tax Class,InterestRate\n")
    req_file.write("Id,Segment,Start Date,Tenor,Amount,Tier,Purpose,Tax Class\n")
    bpl_file.write("Id,Segment,BalancePoolId,Recorded Date,Name,Recorded Amount,Loaned Amount on Recorded Date,Total Amount,Limit Percentage,Total Allocatable Amount\n")

    for i in range(src_count):
        src_date = randint(start_date, end_date-1)
        src_date_str = dateval_to_str(src_date)
        src_tenor = min(max_tenor, randint(1, end_date - src_date))
        src_amount = min_amount + amount_step_size * randint(0, amount_steps)
        src_interest_rate = round(0.07 + random() * (0.12 - 0.07), 2)
        src_file.write("%d,,%s,%d,%d,,,,%.2f\n" %
                (i+1, src_date_str, src_tenor, src_amount, src_interest_rate))

    for i in range(req_count):
        req_date = randint(start_date, end_date-1)
        req_date_str = dateval_to_str(req_date)
        req_tenor = min(max_tenor, randint(1, end_date - req_date))
        req_amount = min_amount + amount_step_size * randint(0, amount_steps)
        req_file.write("%d,,%s,%d,%d,,,,\n" % (i+1, req_date_str, req_tenor, req_amount))

    for i in range(bpl_count):
        bpl_amount = 10 * (min_amount + amount_step_size * randint(0, amount_steps))
        bpl_file.write("%d,,,,,,,,,%d.0\n" % (i+1, bpl_amount))

    src_file.close()
    req_file.close()
    bpl_file.close()


if __name__ == "__main__":
    src_count = 80
    req_count = src_count
    bpl_count = 2
    duration = 24
    max_tenor = duration
    generate("DSg_6", src_count, req_count, bpl_count, duration, max_tenor)
