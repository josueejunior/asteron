# Modelo de Memória Asteron

As regras de ouro que garantem a estabilidade e o paralelismo real da linguagem:

1. **AST e IR são imutáveis**: Após a compilação, a árvore sintática e a representação intermediária tornam-se somente leitura. Tasks podem ler, mas nunca modificar ou liberar essas estruturas.
2. **Scheduler não libera memória**: O Scheduler é um executor, não um dono. Ele coordena o trabalho, mas a responsabilidade de liberar recursos compartilhados é sempre do runtime principal (`main`).
3. **Cada task tem contexto próprio**: Para evitar colisões em execução paralela, cada unidade de trabalho possui seu próprio estado local isolado.
4. **Free só ocorre em pontos bem definidos**: A liberação de memória segue um ciclo de vida previsível e linear, ocorrendo apenas quando nenhuma thread paralela está mais ativa.

