# Trabalho 02 (segunda fase)

Este é o segundo trabalho da segunda fase do EmbarcaTech, e consiste num
Ohmímetro com identificador de cor de resistor (no esquema de 4 bandas),
suportando uma faixa de 510 Ohms a 100 kOhms e descrevendo com uma faixa
de tolerância de 5%. A descrição completa está disponível no documento
enviado pelo Classroom.

Este programa é uma versão modificada do [Ohmímetro feito pelo prof. Wilton Lacerda](https://github.com/wiltonlacerda/EmbarcaTechResU1RevOhm/blob/main/Ohmimetro01.c).

## Execução

Para rodar este código, é possível utilizar o CMake para a compilação (o
`pico_sdk_import.cmake` deve ser providenciado anteriormente - isso pode
ser feito, por exemplo, importando o projeto pelo VSCode), enviando
depois o arquivo `.uf2` para a placa via USB. Com isso, o programa irá
rodar na placa.

Há um diagrama no Wokwi para este código, mas ele não está funcional no
momento.
