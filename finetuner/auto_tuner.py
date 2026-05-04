from pathlib import Path

import pandas as pd

ROOT_DIR = Path(__file__).resolve().parent.parent
eval_constants_file = ROOT_DIR / 'engine' / 'eval_constants.hpp'
auto_tuner_csv = ROOT_DIR / 'finetuner' / 'auto_tuner.csv'
fen_csv = ROOT_DIR / 'finetuner' / 'fen_data.csv'

list_of_weights = []
list_of_fens = []

def parse_weights_from_header(file):
    line = file.find('int32_t')
    name_idx = file.find('int32_t', line)
    value_idx = file.find('=', line)
    end_idx = file.find(';', value_idx)

    while value_idx != -1 and end_idx != -1:
        
        w_name = file[name_idx + len('int32_t') : value_idx - 1].strip()
        w_value = file[value_idx + len('=') : end_idx].strip()

        # Remove any single quotes from number literals (e.g., 1'000 -> 1000)
        if w_value.find("'") != -1:
            w_value = w_value.replace("'", "")


        if w_value.find("{") == -1 and w_value.find("max") == -1:
            w_min = int(int(w_value) - (int(w_value) / 10))
            w_max = int(int(w_value) + (int(w_value) / 10))
            step = int(int(w_value) / 100)
            row = (w_name, w_value, w_min, w_max, step, True, 'default')        
            list_of_weights.append(row)
            

        line = file.find('int32_t', end_idx)
        name_idx = file.find('int32_t', line)
        value_idx = file.find('=', line)
        end_idx = file.find(';', value_idx)


def write_weights_to_csv():
    data = pd.DataFrame(list_of_weights, 
                        columns=['w_name', 'w_default', 'w_min', 'w_max', 'step', 'enabled', 'group'])
    data.to_csv(auto_tuner_csv, index=False)


def parse_fen_from_csv():
    data = pd.read_csv(fen_csv)
    for row in data.itertuples():
        list_of_fens.append((row.fen))



def main():
    # weights_data = pd.read_csv(auto_tuner_csv)

    with eval_constants_file.open('r', encoding='utf-8') as eval_constants:
        parse_weights_from_header(eval_constants.read())

    write_weights_to_csv()



    parse_fen_from_csv()

    
    # Debug only!
    print(*list_of_weights, sep = '\n')

    

main()