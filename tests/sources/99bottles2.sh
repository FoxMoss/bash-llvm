bottles() {
  beer=$1
  (( beer > 0 )) && printf '%d' $beer ||  printf "No more"
  printf " bottle"
  ((beer != 1 )) && printf "s"
  printf " of beer"
}

for ((i=99;i>=0;i--)); do
  ((remaining=i))
  printf '%s on the wall\n' "$(bottles $remaining)"
  printf '%s\n' "$(bottles $remaining)"
  if (( remaining == 0 )); then
    printf 'Go to the store and buy some more\n'
    ((remaining+=99))
  else
    printf 'Take one down, pass it around\n'
    ((remaining--))
  fi
  printf '%s on the wall\n\n'  "$(bottles $remaining)"
done
