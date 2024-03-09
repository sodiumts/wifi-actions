# ESP-32 wifi actions
This is a project, that was made for deepening my knowledge of wifi action frames 
and to understand how to interract with wifi using the ESP-idf
Currently implements beacon advertising frames, deauthorization frames. \
\
This project was made purely for educational purposes.

## Requirements 
This project was originally created in ESP-IDF v5.1-rc2 for use with the ESP-32c3.
It should work for most ESP-32 variants (Check Espressifs [documentation](https://docs.espressif.com/projects/esp-idf) for more information)

## Setup and running
Change the defines in the wifi_tools.c file to the desired values.

Set the respective ESP-32 target:
```bash
idf.py set-target esp32c3
```
Build and flash the project on to the ESP-32 for the specified port:
```bash
idf.py flash -p (port)
```
## License
[MIT](https://choosealicense.com/licenses/mit/)