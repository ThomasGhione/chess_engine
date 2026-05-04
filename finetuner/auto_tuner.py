from pathlib import Path

import pandas as pd

ROOT_DIR = Path(__file__).resolve().parent.parent
eval_constants_file = ROOT_DIR / 'engine' / 'eval_constants.hpp'
csv_file = ROOT_DIR / 'finetuner' / 'auto_tuner.csv'

list_of_constants = []

def build_list_of_constants_from_hpp(file):
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
            pair = (w_name, w_value)        
            list_of_constants.append(pair)
            

        line = file.find('int32_t', end_idx)
        name_idx = file.find('int32_t', line)
        value_idx = file.find('=', line)
        end_idx = file.find(';', value_idx)

def main():
    weights_data = pd.read_csv(csv_file)

    with eval_constants_file.open('r', encoding='utf-8') as eval_constants:
        build_list_of_constants_from_hpp(eval_constants.read())

    print(*list_of_constants, sep = '\n')

    

main()