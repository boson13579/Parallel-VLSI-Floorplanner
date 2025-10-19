### �`�J���R�G`Floorplan` �������ɶ����Ӥj��

�b�@�������h�������N���A�D�n���p��ɶ�����b `perturb()` -> `pack()` -> `calculate_cost()` -> `calculate_inl()` �o�Ӭy�{�W�C�䤤�A`pack()` �M `calculate_inl()` �O�p��̱K���������C

#### 1. `pack()` �禡���R

*   **�γ~**�G�N��H�� B*-Tree ���c�A�z�L�`���u���j�M (DFS) �t��k�A�ഫ���C�Ӱ϶����骺 `(x, y)` ���z�y�СC
*   **�֤ߺt��k**�G�a�������u (Contour Line) ��s�����j DFS�C
*   **�����򤣯�²��a����ơH**
    *   **���j����Ƭ̩ۨ� (Strong Data Dependency)**�G�o�O�̥D�n����]�C�b DFS ���L�{���A�@�Ӹ`�I���y��**�����̿�**�����`�I��**�̲׮y��**�C
        *   ���l�`�I�� x �y�� = ���`�I�� x �y�� + ���`�I���e�סC
        *   �k�l�`�I�� x �y�� = ���`�I�� x �y�СC
        *   �Ҧ��`�I�� y �y�СA���̿��@��**�@�ɪ��B���_�b�ܰ�**�� `contour` ��Ƶ��c�C

    *   **�ڥi�H�� `#pragma omp parallel for` ��Ҧ��`�I���p�⥭��ƶܡH**
        *   **���藍��**�C�Q���@�U�A�p�G�Ҧ�������P�ɶ}�l�p��Ҧ��`�I���y�СC����� A �b�p��`�I `C` ���y�ЮɡA�i�०�����`�I `P` ���y���٨S���Q����� B �p��X�ӡC�o�|�ɭP `C` ���y�Ч������~�C��Ӻt��k�����T�ʳ��إߦb**�`�� (Sequential)** ���X�ݶ��ǤW�C

    *   **����̩ۨ� (Control Dependency)**�GDFS �����j���|�����N�O�`�Ǫ��C�{����������������`�I���B�z�A�~��M�w�U�@�B�O���j�쥪�l���٬O�k�l��C

    *   **����**�G`pack()` ���L�{�N��**�\�j��**�C�A��������U���@�ƪ��j����n�]���`�I�^�A�~��b���̤W����W�@�ơ]�l�`�I�^�C�A���i���� 10 �Ӥu�H�P�ɥh����W�����N�@���j�A�o�|�ɭP�㭱�𪺱Y��C

**����**�G`pack()` �禡�ѩ��t��k���b��**�`�Ǩ̿��**�A�L�k�z�L²�檺 OpenMP ���O�i�業��ơC����չϥ���ƥ����欰���|�}�a�t��k�����T�ʡC

#### 2. `calculate_inl()` �禡���R

�o�Ө禡�󦳽�A�]��������**�T��s�b�i�H����ƪ�����**�A���o�]��n�O�����u������Ӳɫץ���Ƴq�`�O���a�D�N�v���̨νd�ҡC

���ڭ̱N `calculate_inl()` ���p��B�J��Ѷ}�ӡG

*   **�B�J�@�G�p��C�Ӱ϶������I��G�����ߪ��Z������**
    ```cpp
    // for (const auto& node : tree) { ... }
    ```
    *   **�i�H����ƶܡH** **�i�H**�C�C�Ӱ϶����Z���p��O�����W�ߪ��C
    *   **�����򤣳o�򰵡H** **�}�P�L�� (High Overhead)**�C�j�餺�����p��q���p�]�X���[��k�M���k�^�ACPU ����o�ǫ��O�i��u�ݭn�X�̭ө`�� (nanoseconds)�C�ӱҰ� OpenMP ����ϰ�B�إߩM�P�B��������}�P�A�h�O�H�L�� (microseconds) �Ʀܲ@�� (milliseconds) �����C�o�N��**���F�h�@���ѦӽШӤ@��ӷh�a�ζ�**�A�ǳƤu�@���ɶ������W�L�F��ڷh�B���ɶ��C

*   **�B�J�G�G��϶����W�ٱƧ�**
    ```cpp
    // sort(block_dists.begin(), block_dists.end());
    ```
    *   **�i�H����ƶܡH** **�z�פW�i�H**�C�s�b����������ƧǺt��k�C
    *   **�����򤣳o�򰵡H** `std::sort` �����O�@�Ӱ����u�ƪ��`�Ǻt��k�C�n��ʹ�@�@�Ӱ��Ī�����ƧǫD�`�x���C���M C++17 ���᪺�����䴩���浦�� (`std::sort(std::execution::par, ...)`�^�A����󤤤p���W�ҡ]�Ҧp�X�ʭӰ϶��^����ơA��}�P�i�ऴ�M����`�Ǫ����C

*   **�B�J�T�G�p��ֿn�M (Cumulative Sum) `s_actual`**
    ```cpp
    // for (const auto& bd : block_dists) s_actual.push_back(current_sum += bd.dist_sq);
    ```
    *   **�i�H����ƶܡH** **���i�H**�]�ϥ�²���k�^�C
    *   **������H** �o�O�嫬��**�j����a�̩ۨ� (Loop-Carried Dependency)**�C�p�� `s_actual` ���� `i` �Ӥ����A**����**�����D�� `i-1` �Ӥ��������G�C��@�����N����J�A�O�e�@�����N����X�C�o�ب̿�ʬO�L�k�Q `#pragma omp for` ���}���C
    *   **�i������**�G���M�s�b�W���u����e��M�v(Parallel Prefix Sum / Scan) �������t��k�i�H�ѨM�o�Ӱ��D�A��������@�D�`�����A�������A�Ω󦹳B�C

*   **�B�J�|�G�p��u�ʰj�k���U���`�M**
    ```cpp
    // for (int i = 0; i < n; ++i) { sum_x += ...; sum_y += ...; }
    ```
    *   **�i�H����ƶܡH** **�i�H**�C�o�O�@�Ө嫬���k�� (Reduction) �ާ@�C
    *   **�p���@�H** `#pragma omp parallel for reduction(+:sum_x, sum_y, ...)`
    *   **�����򤣳o�򰵡H** ��]�P�B�J�@�G**�}�P�L��**�C�j�餺���p��P�˫D�`�֡A���ȱo�����Ұʥ���ơC

---

### �`�����

| �禡 / �B�J | �ɶ������� (����) | �i²�業���? | �����򤣦� / ���ȱo�H |
| :--- | :--- | :--- | :--- |
| **`pack()`** | O(N), N���϶��� | **�_** | **�ڥ���]**�G�t��k�㦳���b��**��Ƭ̩ۨ�**�M**����̩ۨ�**�C |
| **`calculate_inl()`** | | | |
| ? �p��Z�� | O(N) | **�O** | **�į೴��**�G�����**�}�P**���j��p�⥻�������q�]���Ӳɫס^�C |
| ? �Ƨ� | O(N log N) | �_ | `std::sort` �O�`�Ǫ��A����Ƨǹ�@�����B������֡C |
| ? �p��ֿn�M | O(N) | **�_** | **�ڥ���]**�G�s�b**�j����a�̩ۨ�**�C |
| ? �p��j�k�M | O(N) | **�O** | **�į೴��**�G�����**�}�P**���j��p�⥻�������q�]���Ӳɫס^�C |

�o�ӸԲӪ����R���O�a�ҩ��F�A���z�o�ӱM�סA**�N����ƪ����ߩ�b�󰪼h�Ū������W�O�������T���]�p�M��**�C�o�]�N�O�z�M�׳��i���@�ӫD�`���`�ת��޳N�����I�C