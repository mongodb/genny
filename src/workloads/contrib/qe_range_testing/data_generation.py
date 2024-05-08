import random
import math
import os


def calculate_harmonic_sum(n, alpha):
    total_sum = 0.0
    for i in range(1, n + 1):
        total_sum = total_sum + 1 / math.pow(i, alpha)
    return total_sum


def generate_rc_trans_times(n_docs):
    return [random.randint(0, 10**10) for _ in range(n_docs)]


def prob_round(n):
    r = random.random()
    if n % 1 > r:
        return int(math.ceil(n))
    return int(math.floor(n))


def get_zipf_freqlist(total_count, num_distinct, alpha):
    H = calculate_harmonic_sum(num_distinct, alpha)
    sum_freq = 0
    freq_list = []

    for i in range(num_distinct):
        f_i = prob_round(total_count * (math.pow(i + 1, -alpha) / H))
        freq_list.append(f_i)
        sum_freq += f_i

    if sum_freq != total_count:
        freq_list[0] -= sum_freq - total_count
        print(sum_freq - total_count)
        assert freq_list[0] >= freq_list[1]
    return freq_list


def generate_rc_ages(n_docs):
    data = []
    # SD1
    freqlist1 = get_zipf_freqlist(n_docs * 2 // 5, 10800, 2)
    print(freqlist1[:10])
    values1 = list(range(1, 10801))
    for i in range(10800):
        data += [values1[i]] * freqlist1[i]

    # SD2
    freqlist2 = get_zipf_freqlist(n_docs * 2 // 5, 28800 - 10800, 2)
    print(freqlist2[:10])
    values2 = list(range(28800, 10800, -1))
    for i in range(28800 - 10800):
        data += [values2[i]] * freqlist2[i]

    # SD3
    freqlist3 = get_zipf_freqlist(n_docs // 5, 55000 - 28800, 2)
    print(freqlist3[:10])
    values3 = list(range(28801, 55001))
    for i in range(55000 - 28800):
        data += [values3[i]] * freqlist3[i]

    random.shuffle(data)
    return data


def generate_rc_balances(n_docs):
    data = []
    freqlist = get_zipf_freqlist(n_docs, 1000, 2)
    print(freqlist[:10])
    for i in range(1000):
        for _ in range(freqlist[i]):
            r = random.randint(i * 100000 + 1, (i + 1) * 100000)
            r /= 100.0
            data.append(r)
    random.shuffle(data)
    print(len(data))
    return data


def uniform_int_query(minb, maxb, size):
    low = random.randint(minb, maxb - size + 1)
    return (low, low + size - 1)


def uniform_int_queries(minb, maxb, size, n):
    return [uniform_int_query(minb, maxb, size) for _ in range(n)]


def uniform_int_size_range_query(minb, maxb, minsize, maxsize):
    return uniform_int_query(minb, maxb, random.randint(minsize, maxsize))


def uniform_int_size_range_queries(minb, maxb, minsize, maxsize, n):
    return [
        uniform_int_size_range_query(minb, maxb, minsize, maxsize) for _ in range(n)
    ]


def generate_rc_timestamp_queries(n_queries):
    return {
        "t1": uniform_int_queries(0, 10**10, 3600, n_queries),
        "t2": uniform_int_queries(0, 10**10, 86400, n_queries),
        "t3": uniform_int_queries(0, 10**10, 604800, n_queries),
    }


def generate_rc_age_queries(n_queries):
    q = uniform_int_size_range_queries(540, 28260, 1, 100, n_queries // 2)
    q += uniform_int_size_range_queries(29160, 54000, 1, 100, n_queries // 2)
    random.shuffle(q)
    return {"t": q}


def generate_rc_balance_queries(n_queries):
    return {
        "t1": [
            (t[0] / 100.0, t[1] / 100.0)
            for t in uniform_int_size_range_queries(
                80000000, 100000000, 1, 1000, n_queries
            )
        ],
        "t2": [
            (t[0] / 100.0, t[1] / 100.0)
            for t in uniform_int_size_range_queries(
                80000000, 100000000, 1, 1000000, n_queries
            )
        ],
        "t3": [
            (t[0] / 100.0, t[1] / 100.0)
            for t in uniform_int_size_range_queries(
                80000000, 100000000, 1, 20000000, n_queries
            )
        ],
    }


rc_age_file = "data/rc_ages.txt"
rc_balance_file = "data/rc_balances.txt"
rc_timestamp_file = "data/rc_timestamps.txt"

rc_age_up_file = "data/rc_ages_up.txt"
rc_balance_up_file = "data/rc_balances_up.txt"
rc_timestamp_up_file = "data/rc_timestamps_up.txt"


def generate_rc_all_inserts(basedir, n_docs):
    bals = generate_rc_balances(n_docs)
    with open(basedir + rc_balance_file, "w") as f:
        f.write("\n".join([str(round(bal, 2)) for bal in bals]))
    with open(basedir + rc_balance_up_file, "w") as f:
        f.write("\n".join([str(round(bal, 2)) for bal in set(bals)]))

    ages = generate_rc_ages(n_docs)
    with open(basedir + rc_age_file, "w") as f:
        f.write("\n".join([str(age) for age in ages]))
    with open(basedir + rc_age_up_file, "w") as f:
        f.write("\n".join([str(age) for age in set(ages)]))

    tss = generate_rc_trans_times(n_docs)
    with open(basedir + rc_timestamp_file, "w") as f:
        f.write("\n".join([str(ts) for ts in tss]))
    with open(basedir + rc_timestamp_up_file, "w") as f:
        f.write("\n".join([str(ts) for ts in set(tss)]))


def generate_rc_all_queries(basedir, n_queries):
    bals = generate_rc_balance_queries(n_queries)
    for t, queries in bals.items():
        with open(f"{basedir}/queries/rc_balance_{t}_min.txt", "w") as f:
            f.write("\n".join(str(round(q[0], 2)) for q in queries))
        with open(f"{basedir}/queries/rc_balance_{t}_max.txt", "w") as f:
            f.write("\n".join(str(round(q[1], 2)) for q in queries))

    ages = generate_rc_age_queries(n_queries)
    for t, queries in ages.items():
        with open(f"{basedir}/queries/rc_age_{t}_min.txt", "w") as f:
            f.write("\n".join(str(q[0]) for q in queries))
        with open(f"{basedir}/queries/rc_age_{t}_max.txt", "w") as f:
            f.write("\n".join(str(q[1]) for q in queries))

    tss = generate_rc_timestamp_queries(n_queries)
    for t, queries in tss.items():
        with open(f"{basedir}/queries/rc_timestamp_{t}_min.txt", "w") as f:
            f.write("\n".join(str(q[0]) for q in queries))
        with open(f"{basedir}/queries/rc_timestamp_{t}_max.txt", "w") as f:
            f.write("\n".join(str(q[1]) for q in queries))


def generate_all_data(dirpath, n_docs, n_queries):
    random.seed(42)
    os.makedirs(dirpath + "/data", exist_ok=True)
    os.makedirs(dirpath + "/queries", exist_ok=True)
    print(n_docs)
    generate_rc_all_inserts(dirpath, n_docs)
    generate_rc_all_queries(dirpath, n_queries)
