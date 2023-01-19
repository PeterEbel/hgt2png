# pylint: disable=E1101

# Project:        create_terrain
# Creation Date:  2023-01-19
# Author:         Peter Ebel (peter.ebel@santander.de)
# Objective:      creation of fake data for terrain building using height maps
#
# Modification Log:
# Version Date        Modified By	Modification Details
# 1.0.0   2023-01-19  Ebel          Initial creation of the script

import random
import io
from faker import Faker

# logfile = open('log_data_generation.txt', 'w+')

# def log(message):
#     logfile.write(message)


def main():

    rows = 3601
    cols = 3601
    center_x = 3601
    center_y = 3601
    cell_size = 0
    no_value = 0

    fake = Faker('de_DE')
    terrain = io.open('terrain.txt', 'w+')

  # write header 
    terrain.writelines('COLUMS=' + str(cols) + '\n')
    terrain.writelines('ROWS=' + str(cols) + '\n')
    terrain.writelines('CENTERX=' + str(center_x) + '\n')
    terrain.writelines('CENTERY=' + str(center_y) + '\n')
    terrain.writelines('CELLSIZE=' + str(cell_size) + '\n')
    terrain.writelines('NODATAVALUE=' + str(no_value) + '\n')

    for _ in range(rows):
        heights = [(str(fake.random.randint(2400, 3000)) + ' ') for i in range(3601)]
        heights.append('\n')
        terrain.writelines(heights)
    terrain.close()
  # logfile.close()

if __name__ == '__main__':
    main()