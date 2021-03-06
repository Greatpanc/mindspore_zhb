add_child(sampler)

为给定采样器添加子采样器。子采样器将接收父采样器输出的所有数据，并应用其采样逻辑返回新的采样。
    

**参数：**
    - **sampler** (Sampler)：用于从数据集中选择样本的对象。仅支持内置采样器（DistributedSampler、PKSampler、RandomSampler、SequentialSampler、SubsetRandomSampler、WeightedRandomSampler）。
            
            

**示例：**
    >>> sampler = ds.SequentialSampler(start_index=0, num_samples=3)
    >>> sampler.add_child(ds.RandomSampler(num_samples=2))
    >>> dataset = ds.Cifar10Dataset(cifar10_dataset_dir, sampler=sampler)
    

get_child()
    获取给定采样器的子采样器。 

get_num_samples()

    所有采样器都可以包含num_samples数值（也可以将其设置为None）。
    子采样器可以存在，也可以为None。
    如果存在子采样器，则子采样器计数可以是数值或None。
    这些条件会影响最终的采样结果。
    下表显示了调用此函数的可能结果。

    .. list-table::
        :widths: 25 25 25 25
        :header-rows: 1

        * - 子采样器
          - num_samples
          - child_samples
          - 结果
        * - T
          - x
          - y
          - min(x, y)
        * - T
          - x
          - None
          - x
        * - T
          - None
          - y
          - y
        * - T
          - None
          - None
          - None
        * - None
          - x
          - n/a
          - x
        * - None
          - None
          - n/a
          - None

    **返回：**
        int，样本数，可为None。