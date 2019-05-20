import numpy as np
from scipy.special import softmax

for i in [3, 33, 65]:
    nums = softmax(np.random.rand(i, i))
    with open(f'{i}x{i}', 'w') as f:
        f.write(str(i) + '\n')
        for row in nums:
            for entry in row:
                f.write(str(entry) + ' ')
            f.write('\n')
