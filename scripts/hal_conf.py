import atexit
import os.path
import shutil
import tempfile
import re

Import('env')

file_name = 'stm32f4xx_hal_conf.h'
orig_path = os.path.join(env.PioPlatform().get_package_dir('framework-stm32cube'), 'f4','Drivers','STM32F4xx_HAL_Driver','Inc', file_name)
temp = tempfile.mkdtemp()

shutil.move(orig_path, temp)
shutil.copy2(os.path.join('Inc', file_name), orig_path)

@atexit.register
def restore():
    os.remove(orig_path)
    shutil.move(os.path.join(temp, file_name), orig_path)
    shutil.rmtree(temp)
