#!/usr/bin/env bash
declare -A pinyin=(
  [甲]='jiă'
  [乙]='yĭ'
  [丙]='bĭng'
  [丁]='dīng'
  [戊]='wù'
  [己]='jĭ'
  [庚]='gēng'
  [辛]='xīn'
  [壬]='rén'
  [癸]='gŭi'

  [子]='zĭ'
  [丑]='chŏu'
  [寅]='yín'
  [卯]='măo'
  [辰]='chén'
  [巳]='sì'
  [午]='wŭ'
  [未]='wèi'
  [申]='shén'
  [酉]='yŏu'
  [戌]='xū'
  [亥]='hài'
)

celestial=(甲 乙 丙 丁 戊 己 庚 辛 壬 癸)
terrestrial=(子 丑 寅 卯 辰 巳 午 未 申 酉 戌 亥)
animals=(Rat   Ox   Tiger  Rabbit  Dragon Snake
         Horse Goat Monkey Rooster Dog    Pig)
elements=(Wood Fire Earth Metal Water)
aspects=(yang yin)

BaseYear=4

function main {
  if (( !$# )); then
    set -- $(date +%Y)
  fi
  local year
  for year; do
    if (( $# > 1 )); then
      printf '%s:' "$year"
    fi
    local -i cycle_year=year-BaseYear
    local -i stem_number=$cycle_year%${#celestial[@]}
    local stem_han=${celestial[$stem_number]}
    local stem_pinyin=${pinyin[$stem_han]}

    local -i element_number=stem_number/2
    local element=${elements[$element_number]}

    local -i branch_number=$cycle_year%${#terrestrial[@]}
    local branch_han=${terrestrial[$branch_number]}
    local branch_pinyin=${pinyin[$branch_han]}
    local animal=${animals[$branch_number]}

    local -i aspect_number=$cycle_year%${#aspects[@]}
    local aspect=${aspects[$aspect_number]}

    printf '%s%s'  $stem_han $branch_han
    printf '(%s-%s, %s %s; %s)\n' $stem_pinyin $branch_pinyin $element $animal $aspect
  done
}

main "$@"
