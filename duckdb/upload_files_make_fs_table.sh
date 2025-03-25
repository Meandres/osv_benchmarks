#!/usr/bin/env bash

nvme_id=$1
data_folder=$2

ls -l $data_folder | awk -F " " -e 'BEGIN { slba = 0 } NR!=1 {size = $5 % 512 == 0 ? $5/512 : ($5+512)/512; printf "%s %u %u\n", $9, slba, $5; slba+=size}' > nvme_files.txt

echo "nvme_files.txt: "
cat nvme_files.txt
read -p "Write it to nvme $nvme_id ? (Y/N): " confirm && [[ $confirm == [yY] ]] || exit 1

./../../scripts/bind_nvme.sh nvme $nvme_id

nvme_blkdev="/dev/$(ls /sys/bus/pci/devices/0000:$nvme_id/nvme)n1"

nvme format -f $nvme_blkdev

while IFS=" " read -r filename slba size; do
  nlb=$(echo $size | awk -e '{nlb = $1 % 512 == 0 ? $1/512 : ($1+512)/512; printf "%u\n", nlb;}')
  echo "Writing $filename to $slba with size $size (nlb: $nlb)"
  dd if=$data_folder/$filename of=$nvme_blkdev bs=512 count=$nlb seek=$slba status=progress
done < nvme_files.txt

./../../scripts/bind_nvme.sh vfio $nvme_id
