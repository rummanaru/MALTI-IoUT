.venv\Scripts\Activate.ps1

python iout_ml_train_and_export.py --mode full                      # all 4 (default)
python iout_ml_train_and_export.py --mode full --targets delay      # just Delay
python iout_ml_train_and_export.py --mode full --targets delay,jitter
python iout_ml_train_and_export.py --mode cumulative --targets jitter  # applies there too