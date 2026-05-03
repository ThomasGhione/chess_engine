from pathlib import Path

import pandas as pd

ROOT_DIR = Path(__file__).resolve().parent.parent
eval_constants_file = ROOT_DIR / 'engine' / 'eval_constants.hpp'
csv_file = ROOT_DIR / 'finetuner' / 'auto_tuner.csv'

list_of_constants = []

def build_list_of_constants_from_hpp(file):
    line = file.find('int32_t')
    start_idx = file.find('=', line)
    end_idx = file.find(';', start_idx)

    while start_idx != -1 and end_idx != -1:
        eval_data = file[start_idx + 1:end_idx].strip()

        if eval_data.find("'") != -1:
            eval_data = eval_data.replace("'", "")

        if eval_data.find("{") == -1 and eval_data.find("max") == -1:        
            list_of_constants.append(eval_data)

        line = file.find('int32_t', end_idx)
        start_idx = file.find('=', line)
        end_idx = file.find(';', start_idx)

def main():
    weights_data = pd.read_csv(csv_file)

    with eval_constants_file.open('r', encoding='utf-8') as eval_constants:
        build_list_of_constants_from_hpp(eval_constants.read())

    print(*list_of_constants, sep = '\n')

    

main()